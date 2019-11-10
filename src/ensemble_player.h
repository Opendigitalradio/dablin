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

#ifndef ENSEMBLE_PLAYER_H_
#define ENSEMBLE_PLAYER_H_

#include <stdio.h>
#include <stdint.h>
#include <mutex>
#include <string>
#include <thread>

#include "subchannel_sink.h"
#include "dab_decoder.h"
#include "dabplus_decoder.h"
#include "pcm_output.h"
#include "tools.h"

#ifndef DABLIN_DISABLE_SDL
#include "sdl_output.h"
#endif


// --- EnsemblePlayerObserver -----------------------------------------------------------------
class EnsemblePlayerObserver {
public:
	virtual ~EnsemblePlayerObserver() {}

	virtual void EnsembleChangeFormat(const AUDIO_SERVICE_FORMAT& /*format*/) {}
	virtual void EnsembleProcessFIC(const uint8_t* /*data*/, size_t /*len*/) {}
	virtual void EnsembleProcessPAD(const uint8_t* /*xpad_data*/, size_t /*xpad_len*/, bool /*exact_xpad_len*/, const uint8_t* /*fpad_data*/) {}
};


// --- EnsemblePlayer -----------------------------------------------------------------
class EnsemblePlayer : SubchannelSinkObserver, UntouchedStreamConsumer {
protected:
	bool untouched_output;
	bool disable_int_catch_up;
	EnsemblePlayerObserver *observer;

	std::chrono::steady_clock::time_point next_frame_time;

	std::mutex audio_service_mutex;
	AUDIO_SERVICE audio_service;

	SubchannelSink *dec;
	AudioOutput *out;

	virtual void DecodeFrame(const uint8_t *ensemble_frame) = 0;

	void FormatChange(const AUDIO_SERVICE_FORMAT& format);
	void StartAudio(int samplerate, int channels, bool float32) {if(out) out->StartAudio(samplerate, channels, float32);}
	void PutAudio(const uint8_t *data, size_t len) {if(out) out->PutAudio(data, len);}

	void ProcessFIC(const uint8_t *data, size_t len);
	void ProcessPAD(const uint8_t *xpad_data, size_t xpad_len, bool exact_xpad_len, const uint8_t *fpad_data);
	void ProcessUntouchedStream(const uint8_t* data, size_t len, size_t duration_ms);

	void AudioError(const std::string& hint);
	void AudioWarning(const std::string& hint);
	void FECInfo(int total_corr_count, bool uncorr_errors);
public:
	EnsemblePlayer(bool pcm_output, bool untouched_output, bool disable_int_catch_up, EnsemblePlayerObserver *observer);
	~EnsemblePlayer();

	void ProcessFrame(const uint8_t *data);

	bool IsSameAudioService(const AUDIO_SERVICE& audio_service);
	void SetAudioService(const AUDIO_SERVICE& audio_service);

	std::string GetUntouchedStreamFileExtension() {return dec ? dec->GetUntouchedStreamFileExtension() : "";}
	void AddUntouchedStreamConsumer(UntouchedStreamConsumer* consumer) {if(dec) dec->AddUntouchedStreamConsumer(consumer);};
	void RemoveUntouchedStreamConsumer(UntouchedStreamConsumer* consumer) {if(dec) dec->RemoveUntouchedStreamConsumer(consumer);};

	void StopAudio() {if(out) out->StopAudio();}
	void SetAudioMute(bool audio_mute) {if(out) out->SetAudioMute(audio_mute);}
	void SetAudioVolume(double audio_volume) {if(out) out->SetAudioVolume(audio_volume);}
	bool HasAudioVolumeControl() {return out ? out->HasAudioVolumeControl() : false;}
};

#endif /* ENSEMBLE_PLAYER_H_ */
