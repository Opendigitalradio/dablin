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

#ifndef DABLIN_GTK_H_
#define DABLIN_GTK_H_

#include <algorithm>
#include <mutex>
#include <queue>
#include <signal.h>
#include <string>
#include <thread>
#include <time.h>

#include <gtkmm.h>

#include "dablin_gtk_sls.h"
#include "eti_source.h"
#include "eti_player.h"
#include "fic_decoder.h"
#include "pad_decoder.h"
#include "tools.h"
#include "version.h"

#define WIDGET_SPACE 5



// --- DABlinGTKChannelColumns -----------------------------------------------------------------
class DABlinGTKChannelColumns : public Gtk::TreeModelColumnRecord {
public:
	Gtk::TreeModelColumn<Glib::ustring> col_string;
	Gtk::TreeModelColumn<DAB_LIVE_SOURCE_CHANNEL> col_channel;

	DABlinGTKChannelColumns() {
		add(col_string);
		add(col_channel);
	}
};


// --- DABlinGTKServiceColumns -----------------------------------------------------------------
class DABlinGTKServiceColumns : public Gtk::TreeModelColumnRecord {
public:
	Gtk::TreeModelColumn<Glib::ustring> col_string;
	Gtk::TreeModelColumn<LISTED_SERVICE> col_service;

	DABlinGTKServiceColumns() {
		add(col_string);
		add(col_service);
	}
};


// --- DABlinGTKOptions -----------------------------------------------------------------
struct DABlinGTKOptions {
	std::string filename;
	std::string initial_label;
	int initial_sid;
	int initial_scids;
	std::string dab_live_source_binary;
	std::string dab_live_source_type;
	std::string displayed_channels;
	std::string initial_channel;
	std::string recordings_path;
	bool pcm_output;
	bool untouched_output;
	int gain;
	bool initially_disable_slideshow;
	bool loose;
	
DABlinGTKOptions() :
	initial_sid(LISTED_SERVICE::sid_none),
	initial_scids(LISTED_SERVICE::scids_none),
	dab_live_source_type(DABLiveETISource::TYPE_DAB2ETI),
	recordings_path("/tmp"),
	pcm_output(false),
	untouched_output(false),
	gain(DAB_LIVE_SOURCE_CHANNEL::auto_gain),
	initially_disable_slideshow(false),
	loose(false)
	{}
};


// --- GTKDispatcherQueue -----------------------------------------------------------------
template<typename T>
class GTKDispatcherQueue {
private:
	Glib::Dispatcher dispatcher;
	std::mutex mutex;
	std::queue<T> values;
public:
	Glib::Dispatcher& GetDispatcher() {return dispatcher;}

	void PushAndEmit(T value) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			values.push(value);
		}
		dispatcher.emit();
	}

	T Pop() {
		std::lock_guard<std::mutex> lock(mutex);

		T value = values.front();
		values.pop();
		return value;
	}
};


// --- DABlinGTK -----------------------------------------------------------------
class DABlinGTK : public Gtk::Window, ETISourceObserver, ETIPlayerObserver, FICDecoderObserver, PADDecoderObserver, UntouchedStreamConsumer {
private:
	DABlinGTKOptions options;

	Gtk::ListStore::iterator initial_channel_it;
	bool initial_channel_appended;

	DABlinGTKSlideshowWindow slideshow_window;

	ETISource *eti_source;
	std::thread eti_source_thread;

	ETIPlayer *eti_player;

	FICDecoder *fic_decoder;
	PADDecoder *pad_decoder;

	std::mutex rec_file_mutex;
	FILE* rec_file;

	// ETI data change
	GTKDispatcherQueue<ETI_PROGRESS> eti_update_progress;
	void ETIProcessFrame(const uint8_t *data) {eti_player->ProcessFrame(data);}
	void ETIUpdateProgress(const ETI_PROGRESS& progress) {eti_update_progress.PushAndEmit(progress);}
	void ETIUpdateProgressEmitted();

	GTKDispatcherQueue<AUDIO_SERVICE_FORMAT> eti_change_format;
	void ETIChangeFormat(const AUDIO_SERVICE_FORMAT& format) {eti_change_format.PushAndEmit(format);}
	void ETIChangeFormatEmitted();

	void ETIProcessFIC(const uint8_t *data, size_t len) {fic_decoder->Process(data, len);}
	void ETIResetFIC() {fic_decoder->Reset();}
	void ETIProcessPAD(const uint8_t *xpad_data, size_t xpad_len, bool exact_xpad_len, const uint8_t* fpad_data) {pad_decoder->Process(xpad_data, xpad_len, exact_xpad_len, fpad_data);}

	void ProcessUntouchedStream(const uint8_t* data, size_t len, size_t /*duration_ms*/);


	Gtk::Grid top_grid;

	Gtk::Frame frame_combo_channels;
	DABlinGTKChannelColumns combo_channels_cols;
	Glib::RefPtr<Gtk::ListStore> combo_channels_liststore;
	Gtk::ComboBox combo_channels;
	int ComboChannelsSlotCompare(const Gtk::TreeModel::iterator& a, const Gtk::TreeModel::iterator& b);

	Gtk::Frame frame_label_ensemble;
	Gtk::Label label_ensemble;

	Gtk::Frame frame_combo_services;
	DABlinGTKServiceColumns combo_services_cols;
	Glib::RefPtr<Gtk::ListStore> combo_services_liststore;
	Gtk::ComboBox combo_services;
	int ComboServicesSlotCompare(const Gtk::TreeModel::iterator& a, const Gtk::TreeModel::iterator& b);

	Gtk::Frame frame_label_format;
	Gtk::Label label_format;

	Gtk::ToggleButton tglbtn_record;
	Gtk::ToggleButton tglbtn_mute;
	Gtk::VolumeButton vlmbtn;
	Gtk::ToggleButton tglbtn_slideshow;

	Gtk::Frame frame_label_dl;
	Gtk::Label label_dl;

	Gtk::ProgressBar progress_position;


	void InitWidgets();
	void AddChannels();
	void AddChannel(dab_channels_t::const_iterator &it, int gain);

	void SetService(const LISTED_SERVICE& service);

	void on_tglbtn_record();
	void on_tglbtn_mute();
	void on_vlmbtn(double value);
	void on_tglbtn_slideshow();
	void on_combo_channels();
	void on_combo_services();
	bool on_window_delete_event(GdkEventAny* any_event);

	void ConnectKeyPressEventHandler(Gtk::Widget& widget);
	bool HandleKeyPressEvent(GdkEventKey* key_event);
	bool HandleConfigureEvent(GdkEventConfigure* configure_event);
	void TryServiceSwitch(int index);

	// FIC data change
	GTKDispatcherQueue<FIC_ENSEMBLE> fic_change_ensemble;
	void FICChangeEnsemble(const FIC_ENSEMBLE& ensemble) {fic_change_ensemble.PushAndEmit(ensemble);}
	void FICChangeEnsembleEmitted();

	GTKDispatcherQueue<LISTED_SERVICE> fic_change_service;
	void FICChangeService(const LISTED_SERVICE& service) {fic_change_service.PushAndEmit(service);}
	void FICChangeServiceEmitted();

	void FICDiscardedFIB();

	// PAD data change
	GTKDispatcherQueue<DL_STATE> pad_change_dynamic_label;
	void PADChangeDynamicLabel(const DL_STATE& dl) {pad_change_dynamic_label.PushAndEmit(dl);}
	void PADChangeDynamicLabelEmitted();

	GTKDispatcherQueue<MOT_FILE> pad_change_slide;
	void PADChangeSlide(const MOT_FILE& slide) {pad_change_slide.PushAndEmit(slide);}
	void PADChangeSlideEmitted();

	void PADLengthError(size_t announced_xpad_len, size_t xpad_len);

	Glib::ustring DeriveShortLabel(Glib::ustring long_label, uint16_t short_label_mask);
public:
	DABlinGTK(DABlinGTKOptions options);
	~DABlinGTK();
};



#endif /* DABLIN_GTK_H_ */
