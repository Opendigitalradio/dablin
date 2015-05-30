/*
    DABlin - capital DAB experience
    Copyright (C) 2015 Stefan Pöschel

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

#include <thread>

#include <gtkmm.h>

#include "eti_player.h"
#include "fic_decoder.h"
#include "pad_decoder.h"

#define WIDGET_SPACE 5

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


// --- DABlinGTK -----------------------------------------------------------------
class DABlinGTK : public Gtk::Window, ETIPlayerObserver, FICDecoderObserver, PADDecoderObserver {
private:
	int initial_sid;

	ETIPlayer *eti_player;
	std::thread eti_player_thread;

	FICDecoder *fic_decoder;
	PADDecoder *pad_decoder;

	Glib::Dispatcher format_change;
	void ETIChangeFormat() {format_change.emit();}
	void ETIChangeFormatEmitted();

	void ETIProcessFIC(const uint8_t *data, size_t len) {fic_decoder->Process(data, len);}
	void ETIProcessPAD(const uint8_t *xpad_data, size_t xpad_len, uint16_t fpad) {pad_decoder->Process(xpad_data, xpad_len, fpad);}
	void ETIResetPAD() {pad_decoder->Reset();}

	Gtk::Grid top_grid;

	Gtk::Frame frame_label_ensemble;
	Gtk::Label label_ensemble;


	Gtk::Frame frame_combo_services;
	DABlinGTKServiceColumns combo_services_cols;
	Glib::RefPtr<Gtk::ListStore> combo_services_liststore;
	Gtk::ComboBox combo_services;


	Gtk::Frame frame_label_format;
	Gtk::Label label_format;

	Gtk::ToggleButton tglbtn_mute;

	Gtk::Frame frame_label_dl;
	Gtk::Label label_dl;

	void SetService(SERVICE service);

	void on_tglbtn_mute();
	void on_combo_services();

	// FIC data change
	Glib::Dispatcher fic_data_change_ensemble;
	void FICChangeEnsemble() {fic_data_change_ensemble.emit();}
	void FICChangeEnsembleEmitted();

	Glib::Dispatcher fic_data_change_services;
	void FICChangeServices() {fic_data_change_services.emit();}
	void FICChangeServicesEmitted();

	// PAD data change
	Glib::Dispatcher pad_data_change_dynamic_label;
	void PADChangeDynamicLabel() {pad_data_change_dynamic_label.emit();}
	void PADChangeDynamicLabelEmitted();
public:
	DABlinGTK(std::string filename, int initial_sid);
	~DABlinGTK();
};



#endif /* DABLIN_GTK_H_ */
