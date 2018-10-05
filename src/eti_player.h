/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2018 Stefan Pöschel

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

#ifndef ETI_PLAYER_H_
#define ETI_PLAYER_H_

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


// --- ETIPlayerObserver -----------------------------------------------------------------
class ETIPlayerObserver {
public:
	virtual ~ETIPlayerObserver() {}

	virtual void ETIChangeFormat(const std::string& /*format*/) {}
	virtual void ETIProcessFIC(const uint8_t* /*data*/, size_t /*len*/) {}
	virtual void ETIProcessPAD(const uint8_t* /*xpad_data*/, size_t /*xpad_len*/, bool /*exact_xpad_len*/, const uint8_t* /*fpad_data*/) {}
};


// --- ETIPlayer -----------------------------------------------------------------
class ETIPlayer : SubchannelSinkObserver, UntouchedStreamConsumer {
private:
	bool untouched_output;
	ETIPlayerObserver *observer;

	std::chrono::steady_clock::time_point next_frame_time;
	uint32_t prev_fsync;

	std::mutex audio_service_mutex;
	AUDIO_SERVICE audio_service;

	SubchannelSink *dec;
	AudioOutput *out;

	void DecodeFrame(const uint8_t *eti_frame);

	void FormatChange(const std::string& format);
	void StartAudio(int samplerate, int channels, bool float32) {if(out) out->StartAudio(samplerate, channels, float32);}
	void PutAudio(const uint8_t *data, size_t len) {if(out) out->PutAudio(data, len);}

	void ProcessFIC(const uint8_t *data, size_t len);
	void ProcessPAD(const uint8_t *xpad_data, size_t xpad_len, bool exact_xpad_len, const uint8_t *fpad_data);
	void ProcessUntouchedStream(const uint8_t* data, size_t len);

	void AudioError(const std::string& hint);
	void AudioWarning(const std::string& hint);
	void FECInfo(int total_corr_count, bool uncorr_errors);
public:
	ETIPlayer(bool pcm_output, bool untouched_output, ETIPlayerObserver *observer);
	~ETIPlayer();

	void ProcessFrame(const uint8_t *data);

	bool IsSameAudioService(const AUDIO_SERVICE& audio_service);
	void SetAudioService(const AUDIO_SERVICE& audio_service);

	std::string GetUntouchedStreamFileExtension() {return dec ? dec->GetUntouchedStreamFileExtension() : "";}
	void AddUntouchedStreamConsumer(UntouchedStreamConsumer* consumer) {if(dec) dec->AddUntouchedStreamConsumer(consumer);};
	void RemoveUntouchedStreamConsumer(UntouchedStreamConsumer* consumer) {if(dec) dec->RemoveUntouchedStreamConsumer(consumer);};

	void SetAudioMute(bool audio_mute) {if(out) out->SetAudioMute(audio_mute);}
	void SetAudioVolume(double audio_volume) {if(out) out->SetAudioVolume(audio_volume);}
	bool HasAudioVolumeControl() {return out ? out->HasAudioVolumeControl() : false;}
};



#endif /* ETI_PLAYER_H_ */
