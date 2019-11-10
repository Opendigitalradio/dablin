/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2019 Stefan Pöschel

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ensemble_player.h"


// --- EnsemblePlayer -----------------------------------------------------------------
EnsemblePlayer::EnsemblePlayer(bool pcm_output, bool untouched_output, bool disable_int_catch_up, EnsemblePlayerObserver *observer) {
	this->untouched_output = untouched_output;
	this->disable_int_catch_up = disable_int_catch_up;
	this->observer = observer;

	dec = nullptr;
	out = nullptr;

	if(!untouched_output) {
#ifndef DABLIN_DISABLE_SDL
		if(!pcm_output)
			out = new SDLOutput;
		else
#endif
			out = new PCMOutput;
	}
}

EnsemblePlayer::~EnsemblePlayer() {
	delete dec;
	delete out;
}

bool EnsemblePlayer::IsSameAudioService(const AUDIO_SERVICE& audio_service) {
	std::lock_guard<std::mutex> lock(audio_service_mutex);

	// check, if already the same service
	return this->audio_service == audio_service;
}

void EnsemblePlayer::SetAudioService(const AUDIO_SERVICE& audio_service) {
	std::lock_guard<std::mutex> lock(audio_service_mutex);

	// abort, if already the same service
	if(this->audio_service == audio_service)
		return;

	// cleanup - audio only stopped (externally) on ensemble change!
	if(dec) {
		delete dec;
		dec = nullptr;
	}

	if(audio_service.IsNone())
		fprintf(stderr, "EnsemblePlayer: playing nothing\n");
	else
		fprintf(stderr, "EnsemblePlayer: playing sub-channel %d (%s)\n", audio_service.subchid, audio_service.dab_plus ? "DAB+" : "DAB");

	// append - use more precise float32 output (if supported by decoder)
	if(!audio_service.IsNone()) {
		if(audio_service.dab_plus)
			dec = new SuperframeFilter(this, !untouched_output, true);
		else
			dec = new MP2Decoder(this, true);
		if(untouched_output)
			dec->AddUntouchedStreamConsumer(this);
	}

	this->audio_service = audio_service;
}

void EnsemblePlayer::ProcessFrame(const uint8_t *data) {
	bool init = next_frame_time.time_since_epoch().count() == 0;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	// flow control
	if(init || (disable_int_catch_up && now > next_frame_time + std::chrono::milliseconds(24))) {
		// resync, if desired and noticeable after expected arrival of next frame
		if(!init)
			fprintf(stderr, "EnsemblePlayer: resynced to stream\n");
		next_frame_time = now;
	} else {
		// otherwise just sleep until expected arrival (if not yet reached)
		std::this_thread::sleep_until(next_frame_time);
	}
	next_frame_time += std::chrono::milliseconds(24);

	DecodeFrame(data);
}

void EnsemblePlayer::FormatChange(const AUDIO_SERVICE_FORMAT& format) {
	fprintf(stderr, "EnsemblePlayer: format: %s\n", format.GetSummary().c_str());
	if(observer)
		observer->EnsembleChangeFormat(format);
}

void EnsemblePlayer::ProcessFIC(const uint8_t *data, size_t len) {
//	fprintf(stderr, "Received %zu bytes FIC\n", len);
	if(observer)
		observer->EnsembleProcessFIC(data, len);
}

void EnsemblePlayer::ProcessPAD(const uint8_t *xpad_data, size_t xpad_len, bool exact_xpad_len, const uint8_t *fpad_data) {
//	fprintf(stderr, "Received %zu bytes X-PAD\n", xpad_len);
	if(observer)
		observer->EnsembleProcessPAD(xpad_data, xpad_len, exact_xpad_len, fpad_data);
}

void EnsemblePlayer::ProcessUntouchedStream(const uint8_t* data, size_t len, size_t /*duration_ms*/) {
	if(untouched_output) {
		if(fwrite(data, len, 1, stdout) != 1)
			perror("EnsemblePlayer: error while writing untouched stream to stdout");
	}
}

void EnsemblePlayer::AudioError(const std::string& hint) {
	fprintf(stderr, "\x1B[31m" "(%s)" "\x1B[0m" " ", hint.c_str());
}

void EnsemblePlayer::AudioWarning(const std::string& hint) {
	fprintf(stderr, "\x1B[35m" "(%s)" "\x1B[0m" " ", hint.c_str());
}

void EnsemblePlayer::FECInfo(int total_corr_count, bool uncorr_errors) {
	fprintf(stderr, "\x1B[36m" "(%d%s)" "\x1B[0m" " ", total_corr_count, uncorr_errors ? "+" : "");
}
