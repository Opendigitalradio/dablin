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

#include "sdl_output.h"

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len) {
	AudioSource* audio_source = (AudioSource*) userdata;
	audio_source->AudioCallback(stream, len);
}


// --- SDLOutput -----------------------------------------------------------------
SDLOutput::SDLOutput() : AudioOutput() {
	audio_device = 0;
	silence_len = 0;

	samplerate = 0;
	channels = 0;
	float32 = false;

	audio_buffer = nullptr;
	audio_start_buffer_size = 0;

	audio_mute = false;
	audio_volume = 1.0;


	// init SDL
	if(SDL_Init(SDL_INIT_AUDIO))
		throw std::runtime_error("SDLOutput: error while SDL_Init: " + std::string(SDL_GetError()));

	SDL_version sdl_version;
	SDL_GetVersion(&sdl_version);
	fprintf(stderr, "SDLOutput: using SDL version '%u.%u.%u'\n", sdl_version.major, sdl_version.minor, sdl_version.patch);
}

SDLOutput::~SDLOutput() {
	StopAudio();
	SDL_Quit();

	delete audio_buffer;
}

void SDLOutput::StopAudio() {
	if(audio_device) {
		SDL_CloseAudioDevice(audio_device);
		fprintf(stderr, "SDLOutput: audio closed\n");
		audio_device = 0;
	}
}

void SDLOutput::StartAudio(int samplerate, int channels, bool float32) {
	// if no change, do quick restart
	if(audio_device && this->samplerate == samplerate && this->channels == channels && this->float32 == float32) {
		std::lock_guard<std::mutex> lock(audio_buffer_mutex);
		audio_buffer->Clear();
		SetAudioStartBufferSize();
		return;
	}
	this->samplerate = samplerate;
	this->channels = channels;
	this->float32 = float32;

	StopAudio();

	// (re)init buffer
	{
		std::lock_guard<std::mutex> lock(audio_buffer_mutex);

		if(audio_buffer)
			delete audio_buffer;

		// use 500ms buffer
		size_t buffersize = samplerate / 2 * channels * (float32 ? 4 : 2);
		fprintf(stderr, "SDLOutput: using audio buffer of %zu bytes\n", buffersize);
		audio_buffer = new CircularBuffer(buffersize);
		SetAudioStartBufferSize();
	}

	// init audio
	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;
	desired.freq = samplerate;
	desired.format = float32 ? AUDIO_F32SYS : AUDIO_S16SYS;
	desired.channels = channels;
	desired.samples = samplerate * 0.024 * channels;	// DAB frame
	desired.callback = sdl_audio_callback;
	desired.userdata = (AudioSource*) this;

	audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
	if(!audio_device)
		throw std::runtime_error("SDLOutput: error while SDL_OpenAudioDevice: " + std::string(SDL_GetError()));
	fprintf(stderr, "SDLOutput: audio opened; driver name: %s, freq: %d, channels: %d, size: %d, samples: %d, silence: 0x%02X, output: %s\n",
			SDL_GetCurrentAudioDriver(),
			obtained.freq,
			obtained.channels,
			obtained.size,
			obtained.samples,
			obtained.silence,
			float32 ? "32bit float" : "16bit integer");

	audio_spec = obtained;

	SDL_PauseAudioDevice(audio_device, 0);
}

void SDLOutput::SetAudioStartBufferSize() {
	// start audio when 1/4 filled
	audio_start_buffer_size = audio_buffer->Capacity() / 4;
}

void SDLOutput::PutAudio(const uint8_t *data, size_t len) {
	std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	size_t capa = audio_buffer->Capacity() - audio_buffer->Size();
//	if(capa < len) {
//		fprintf(stderr, "SDLOutput: audio buffer overflow, therefore cleaning buffer!\n");
//		audio_buffer->Clear();
//		capa = audio_buffer->capacity();
//	}

	if(len > capa)
		fprintf(stderr, "SDLOutput: audio buffer overflow: %zu > %zu\n", len, capa);

	audio_buffer->Write(data, len);

//	fprintf(stderr, "Buffer: %zu / %zu\n", audio_buffer->Size(), audio_buffer->Capacity());
}

void SDLOutput::AudioCallback(Uint8* stream, int len) {
	// audio
	int filled = GetAudio(stream, len);
	if(filled && silence_len) {
		fprintf(stderr, "SDLOutput: silence ended (%d bytes)\n", silence_len);
		silence_len = 0;
	}

	// silence, if needed
	if(filled < len) {
		int bytes = len - filled;
		memset(stream + filled, audio_spec.silence, bytes);

		if(silence_len == 0)
			fprintf(stderr, "SDLOutput: silence started...\n");
		silence_len += bytes;
	}
}

size_t SDLOutput::GetAudio(uint8_t *data, size_t len) {
	std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	if(audio_start_buffer_size && audio_buffer->Size() >= audio_start_buffer_size)
		audio_start_buffer_size = 0;

	// output silence, if needed
	if(audio_volume == 0.0 || audio_mute || audio_start_buffer_size) {
		if(audio_start_buffer_size == 0)
			audio_buffer->Read(nullptr, len);
		memset(data, audio_spec.silence, len);
		return len;
	}

	// output buffer, if full volume
	if(audio_volume == 1.0)
		return audio_buffer->Read(data, len);

	// output buffer after volume adjustment
	audio_mix_buffer.resize(len);
	size_t got_len = audio_buffer->Read(&audio_mix_buffer[0], len);

	memset(data, audio_spec.silence, got_len);
	SDL_MixAudioFormat(data, &audio_mix_buffer[0], audio_spec.format, got_len, SDL_MIX_MAXVOLUME * audio_volume);

	return got_len;
}
