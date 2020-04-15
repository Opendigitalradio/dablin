/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2020 Stefan Pöschel

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
#include <list>
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
#include "edi_source.h"
#include "edi_player.h"
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
	std::string source_format;
	bool initial_first_found_service;
	std::string initial_label;
	int initial_sid;
	int initial_scids;
	std::string dab_live_source_binary;
	std::string dab_live_source_type;
	std::string displayed_channels;
	std::string initial_channel;
	std::string recordings_path;
	size_t rec_prebuffer_size_s;
	bool pcm_output;
	bool untouched_output;
	bool disable_int_catch_up;
	int gain;
	bool initially_disable_slideshow;
	bool loose;
	bool disable_dyn_fic_msgs;
	
DABlinGTKOptions() :
	source_format(EnsembleSource::FORMAT_ETI),
	initial_first_found_service(false),
	initial_sid(LISTED_SERVICE::sid_none),
	initial_scids(LISTED_SERVICE::scids_none),
	dab_live_source_type(DABLiveETISource::TYPE_DAB2ETI),
	recordings_path("/tmp"),
	rec_prebuffer_size_s(0),
	pcm_output(false),
	untouched_output(false),
	disable_int_catch_up(false),
	gain(DAB_LIVE_SOURCE_CHANNEL::auto_gain),
	initially_disable_slideshow(false),
	loose(false),
	disable_dyn_fic_msgs(false)
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


// --- RecSample -----------------------------------------------------------------
struct RecSample {
	std::vector<uint8_t> data;
	time_t ts;
	size_t duration_ms;

	RecSample(const uint8_t* data, size_t len, size_t duration_ms) {
		this->data.resize(len);
		memcpy(&this->data[0], data, len);

		ts = time(nullptr);
		if(ts == (time_t) -1)
			perror("DABlinGTK: error while getting time for sample");

		this->duration_ms = duration_ms;
	}
};

typedef std::list<RecSample> rec_samples_t;


// --- DABlinGTK -----------------------------------------------------------------
class DABlinGTK : public Gtk::Window, EnsembleSourceObserver, EnsemblePlayerObserver, FICDecoderObserver, PADDecoderObserver, UntouchedStreamConsumer {
private:
	DABlinGTKOptions options;

	Gtk::ListStore::iterator initial_channel_it;

	std::string switch_service_label;
	int switch_service_sid;
	int switch_service_scids;
	bool switch_service_applied;

	DABlinGTKSlideshowWindow slideshow_window;

	EnsembleSource *ensemble_source;
	std::thread ensemble_source_thread;

	EnsemblePlayer *ensemble_player;

	FICDecoder *fic_decoder;
	PADDecoder *pad_decoder;

	// recording
	std::mutex rec_mutex;
	FILE* rec_file;
	std::string rec_filename;
	long int rec_duration_ms;
	rec_samples_t rec_prebuffer;
	long int rec_prebuffer_filled_ms;

	// date/time
	FIC_DAB_DT utc_dt_curr;
	int dt_lto;
	std::string dt_str_prev;
	FIC_DAB_DT utc_dt_next;
	std::chrono::steady_clock::time_point dt_update;

	Gtk::TreeModel::iterator resume_channel_it;
	int resume_service_sid;
	int resume_service_scids;

	// ensemble progress change
	GTKDispatcherQueue<ENSEMBLE_PROGRESS> ensemble_update_progress;
	void EnsembleProcessFrame(const uint8_t *data) {ensemble_player->ProcessFrame(data);}
	void EnsembleUpdateProgress(const ENSEMBLE_PROGRESS& progress) {ensemble_update_progress.PushAndEmit(progress);}
	void EnsembleUpdateProgressEmitted();
	void EnsembleDoRegularWork();

	// ensemble data change
	GTKDispatcherQueue<AUDIO_SERVICE_FORMAT> ensemble_change_format;
	void EnsembleChangeFormat(const AUDIO_SERVICE_FORMAT& format) {ensemble_change_format.PushAndEmit(format);}
	void EnsembleChangeFormatEmitted();

	void EnsembleProcessFIC(const uint8_t *data, size_t len) {fic_decoder->Process(data, len);}
	void EnsembleResetFIC() {fic_decoder->Reset();}
	void EnsembleProcessPAD(const uint8_t *xpad_data, size_t xpad_len, bool exact_xpad_len, const uint8_t* fpad_data) {pad_decoder->Process(xpad_data, xpad_len, exact_xpad_len, fpad_data);}

	void ProcessUntouchedStream(const uint8_t* data, size_t len, size_t duration_ms);


	Gtk::Grid top_grid;

	Gtk::Frame frame_combo_channels;
	Gtk::Box channels_box;
	DABlinGTKChannelColumns combo_channels_cols;
	Glib::RefPtr<Gtk::ListStore> combo_channels_liststore;
	Gtk::ComboBox combo_channels;
	int ComboChannelsSlotCompare(const Gtk::TreeModel::iterator& a, const Gtk::TreeModel::iterator& b);
	Gtk::Button btn_channels_stop;

	FIC_ENSEMBLE ensemble;
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
	Gtk::Label label_record;

	Gtk::ToggleButton tglbtn_slideshow;
	Gtk::ToggleButton tglbtn_mute;
	Gtk::VolumeButton vlmbtn;

	Gtk::Frame frame_label_dl;
	Gtk::Label label_dl;

	Gtk::Frame frame_label_datetime;
	Gtk::Label label_datetime;

	Gtk::Frame frame_label_asu;
	Gtk::Label label_asu;

	Gtk::ProgressBar progress_position;


	void InitWidgets();
	void AddChannels();
	void AddChannel(const dab_channels_t::value_type& dab_channel, int gain);

	void SetService(const LISTED_SERVICE& service);
	void UpdateAnnouncementSupport(const LISTED_SERVICE& service);
	void ShowDateTime(bool scheduled);

	void on_btn_channels_stop();
	void on_tglbtn_record();
	void on_tglbtn_mute();
	void on_vlmbtn(double value);
	void on_tglbtn_slideshow();
	void on_combo_channels();
	void on_combo_services();
	bool on_window_delete_event(GdkEventAny* any_event);

	bool HandleKeyPressEvent(GdkEventKey* key_event);
	bool HandleConfigureEvent(GdkEventConfigure* configure_event);
	bool CheckForIndexKey(GdkEventKey* key_event, int old_index, int& new_index);
	void TrySwitch(Gtk::ComboBox& combo, Glib::RefPtr<Gtk::ListStore>& combo_liststore, int index);

	GTKDispatcherQueue<bool> do_rec_status_update;
	void DoRecStatusUpdate(bool decoding) {do_rec_status_update.PushAndEmit(decoding);}
	void DoRecStatusUpdateEmitted();
	void UpdateRecStatus(bool decoding);

	GTKDispatcherQueue<std::string> do_datetime_update;
	void DoDateTimeUpdate(const std::string& dt_str) {do_datetime_update.PushAndEmit(dt_str);}
	void DoDateTimeUpdateEmitted();

	// FIC data change
	GTKDispatcherQueue<FIC_ENSEMBLE> fic_change_ensemble;
	void FICChangeEnsemble(const FIC_ENSEMBLE& ensemble);
	void FICChangeEnsembleEmitted();

	GTKDispatcherQueue<LISTED_SERVICE> fic_change_service;
	void FICChangeService(const LISTED_SERVICE& service) {fic_change_service.PushAndEmit(service);}
	void FICChangeServiceEmitted();

	void FICChangeUTCDateTime(const FIC_DAB_DT& utc_dt);
	void FICDiscardedFIB();

	// PAD data change
	GTKDispatcherQueue<DL_STATE> pad_change_dynamic_label;
	void PADChangeDynamicLabel(const DL_STATE& dl) {pad_change_dynamic_label.PushAndEmit(dl);}
	void PADChangeDynamicLabelEmitted();

	GTKDispatcherQueue<MOT_FILE> pad_change_slide;
	void PADChangeSlide(const MOT_FILE& slide) {pad_change_slide.PushAndEmit(slide);}
	void PADChangeSlideEmitted();

	void PADLengthError(size_t announced_xpad_len, size_t xpad_len);
public:
	DABlinGTK(DABlinGTKOptions options);
	~DABlinGTK();
};



#endif /* DABLIN_GTK_H_ */
