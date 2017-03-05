/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2016 Stefan Pöschel

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

#include <mutex>
#include <queue>
#include <signal.h>
#include <thread>

#include <gtkmm.h>

#include "eti_source.h"
#include "eti_player.h"
#include "fic_decoder.h"
#include "pad_decoder.h"
#include "tools.h"
#include "version.h"

#define WIDGET_SPACE 5



// --- DABlinGTKSlideshowWindow -----------------------------------------------------------------
class DABlinGTKSlideshowWindow : public Gtk::Window {
private:
	Gtk::Grid top_grid;
	Gtk::Image image;
	Gtk::LinkButton link_button;
public:
	DABlinGTKSlideshowWindow();

	void TryToShow();
	void UpdateSlide(const MOT_FILE& slide);
	void ClearSlide() {image.clear();}
};


// --- DABlinGTKChannelColumns -----------------------------------------------------------------
class DABlinGTKChannelColumns : public Gtk::TreeModelColumnRecord {
public:
	Gtk::TreeModelColumn<Glib::ustring> col_string;
	Gtk::TreeModelColumn<uint32_t> col_freq;

	DABlinGTKChannelColumns() {
		add(col_string);
		add(col_freq);
	}
};


// --- DABlinGTKServiceColumns -----------------------------------------------------------------
class DABlinGTKServiceColumns : public Gtk::TreeModelColumnRecord {
public:
	Gtk::TreeModelColumn<uint32_t> col_sort;
	Gtk::TreeModelColumn<Glib::ustring> col_string;
	Gtk::TreeModelColumn<SERVICE> col_service;

	DABlinGTKServiceColumns() {
		add(col_sort);
		add(col_string);
		add(col_service);
	}
};


// --- DABlinGTKOptions -----------------------------------------------------------------
struct DABlinGTKOptions {
	std::string filename;
	int initial_sid;
	std::string dab2eti_binary;
	std::string displayed_channels;
	std::string initial_channel;
	bool pcm_output;
	int gain;
	bool initially_disable_slideshow;
	
DABlinGTKOptions() : initial_sid(-1), pcm_output(false), gain(DAB2ETI_AUTO_GAIN), initially_disable_slideshow(false) {}
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


struct ETI_PROGRESS {
	double value;
	std::string text;
};


// --- DABlinGTK -----------------------------------------------------------------
class DABlinGTK : public Gtk::Window, ETISourceObserver, ETIPlayerObserver, FICDecoderObserver, PADDecoderObserver {
private:
	DABlinGTKOptions options;

	Gtk::ListStore::iterator initial_channel_it;
	bool initial_channel_appended;
	unsigned long int progress_next_ms;

	DABlinGTKSlideshowWindow slideshow_window;

	ETISource *eti_source;
	std::thread eti_source_thread;

	ETIPlayer *eti_player;

	FICDecoder *fic_decoder;
	PADDecoder *pad_decoder;

	// ETI data change
	GTKDispatcherQueue<ETI_PROGRESS> eti_update_progress;
	void ETIProcessFrame(const uint8_t *data, size_t count, size_t total);
	void ETIUpdateProgressEmitted();

	GTKDispatcherQueue<std::string> eti_change_format;
	void ETIChangeFormat(const std::string& format) {eti_change_format.PushAndEmit(format);}
	void ETIChangeFormatEmitted();

	void ETIProcessFIC(const uint8_t *data, size_t len) {fic_decoder->Process(data, len);}
	void ETIResetFIC() {fic_decoder->Reset();};
	void ETIProcessPAD(const uint8_t *xpad_data, size_t xpad_len, bool exact_xpad_len, uint16_t fpad) {pad_decoder->Process(xpad_data, xpad_len, exact_xpad_len, fpad);}
	void ETIResetPAD() {pad_decoder->Reset();}


	Gtk::Grid top_grid;

	Gtk::Frame frame_combo_channels;
	DABlinGTKChannelColumns combo_channels_cols;
	Glib::RefPtr<Gtk::ListStore> combo_channels_liststore;
	Gtk::ComboBox combo_channels;

	Gtk::Frame frame_label_ensemble;
	Gtk::Label label_ensemble;

	Gtk::Frame frame_combo_services;
	DABlinGTKServiceColumns combo_services_cols;
	Glib::RefPtr<Gtk::ListStore> combo_services_liststore;
	Gtk::ComboBox combo_services;

	Gtk::Frame frame_label_format;
	Gtk::Label label_format;

	Gtk::ToggleButton tglbtn_mute;
	Gtk::ToggleButton tglbtn_slideshow;

	Gtk::Frame frame_label_dl;
	Gtk::Label label_dl;

	Gtk::ProgressBar progress_position;


	void InitWidgets();
	void AddChannels();
	void AddChannel(dab_channels_t::const_iterator &it);

	void SetService(const SERVICE& service);

	void on_tglbtn_mute();
	void on_tglbtn_slideshow();
	void on_combo_channels();
	void on_combo_services();

	void ConnectKeyPressEventHandler(Gtk::Widget& widget);
	bool HandleKeyPressEvent(GdkEventKey* key_event);

	// FIC data change
	GTKDispatcherQueue<ENSEMBLE> fic_change_ensemble;
	void FICChangeEnsemble(const ENSEMBLE& ensemble) {fic_change_ensemble.PushAndEmit(ensemble);}
	void FICChangeEnsembleEmitted();

	GTKDispatcherQueue<SERVICE> fic_change_service;
	void FICChangeService(const SERVICE& service) {fic_change_service.PushAndEmit(service);}
	void FICChangeServiceEmitted();

	// PAD data change
	GTKDispatcherQueue<DL_STATE> pad_change_dynamic_label;
	void PADChangeDynamicLabel(const DL_STATE& dl) {pad_change_dynamic_label.PushAndEmit(dl);}
	void PADChangeDynamicLabelEmitted();

	GTKDispatcherQueue<MOT_FILE> pad_change_slide;
	void PADChangeSlide(const MOT_FILE& slide) {pad_change_slide.PushAndEmit(slide);}
	void PADChangeSlideEmitted();

	Glib::ustring DeriveShortLabel(Glib::ustring long_label, uint16_t short_label_mask);
	static std::string FramecountToTimecode(size_t value);
public:
	DABlinGTK(DABlinGTKOptions options);
	~DABlinGTK();
};



#endif /* DABLIN_GTK_H_ */
