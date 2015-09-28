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

#include "dablin_gtk.h"

static DABlinGTK *dablin = NULL;

static void break_handler(int param) {
	fprintf(stderr, "...DABlin exits...\n");
	if(dablin)
		dablin->hide();
}


static void usage(const char* exe) {
	fprintf(stderr, "DABlin - plays a DAB(+) subchannel from a frame-aligned ETI(NI) stream via stdin\n");
	fprintf(stderr, "Usage: %s [-d <binary> [-C <ch>,...] [-c <ch>]] [-s <sid>] [file]\n", exe);
	fprintf(stderr, "  -d <binary>   Use dab2eti as source (using the mentioned binary)\n");
	fprintf(stderr, "  -C <ch>,...   Channels to be displayed (separated by comma; requires dab2eti as source)\n");
	fprintf(stderr, "  -c <ch>       Channel to be played (requires dab2eti as source; otherwise no initial channel)\n");
	fprintf(stderr, "  -s <sid>      ID of the service to be played (otherwise no initial service)\n");
	fprintf(stderr, "  file          Input file to be played (stdin, if not specified)\n");
	exit(1);
}


int main(int argc, char **argv) {
	// handle signals
	if(signal(SIGINT, break_handler) == SIG_ERR) {
		perror("DABlin: error while setting SIGINT handler");
		return 1;
	}

	DABlinGTKOptions options;

	// option args
	int c;
	while((c = getopt(argc, argv, "d:C:c:s:")) != -1) {
		switch(c) {
		case 'd':
			options.dab2eti_binary = optarg;
			break;
		case 'C':
			options.displayed_channels = optarg;
			break;
		case 'c':
			options.initial_channel = optarg;
			break;
		case 's':
			options.initial_sid = strtol(optarg, NULL, 0);
			break;
		case '?':
		default:
			usage(argv[0]);
		}
	}

	// non-option args
	switch(argc - optind) {
	case 0:
		break;
	case 1:
		options.filename = argv[optind];
		break;
	default:
		usage(argv[0]);
	}

	// ensure valid options
	if(options.dab2eti_binary.empty()) {
		if(!options.displayed_channels.empty()) {
			fprintf(stderr, "If displayed channels are used, dab2eti must be used!\n");
			usage(argv[0]);
		}
		if(!options.displayed_channels.empty()) {
			fprintf(stderr, "If displayed channels are selected, dab2eti must be used!\n");
			usage(argv[0]);
		}
		if(!options.initial_channel.empty()) {
			fprintf(stderr, "If a channel is selected, dab2eti must be used!\n");
			usage(argv[0]);
		}
	} else {
		if(!options.filename.empty()) {
			fprintf(stderr, "Both a file and dab2eti cannot be used as source!\n");
			usage(argv[0]);
		}
		if(!options.initial_channel.empty() && dab_channels.find(options.initial_channel) == dab_channels.end()) {
			fprintf(stderr, "The channel '%s' is not supported!\n", options.initial_channel.c_str());
			usage(argv[0]);
		}
	}


	fprintf(stderr, "DABlin - capital DAB experience\n");


	int myargc = 1;
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(myargc, argv);

	dablin = new DABlinGTK(options);
	int result = app->run(*dablin);
	delete dablin;

	return result;
}



// --- DABlinGTK -----------------------------------------------------------------
DABlinGTK::DABlinGTK(DABlinGTKOptions options) {
	this->options = options;

	initial_channel_appended = false;

	format_change.connect(sigc::mem_fun(*this, &DABlinGTK::ETIChangeFormatEmitted));
	fic_data_change_ensemble.connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeEnsembleEmitted));
	fic_data_change_services.connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeServicesEmitted));
	pad_data_change_dynamic_label.connect(sigc::mem_fun(*this, &DABlinGTK::PADChangeDynamicLabelEmitted));

	eti_player = new ETIPlayer(this);

	if(!options.dab2eti_binary.empty()) {
		eti_source = NULL;
	} else {
		eti_source = new ETISource(options.filename, this);
		eti_source_thread = std::thread(&ETISource::Main, eti_source);
	}

	fic_decoder = new FICDecoder(this);
	pad_decoder = new PADDecoder(this);

	set_title("DABlin");
	set_default_icon_name("media-playback-start");

	InitWidgets();

	set_border_width(2 * WIDGET_SPACE);
	show_all_children();

	// combobox first must be visible
	if(combo_channels_liststore->iter_is_valid(initial_channel_it))
		combo_channels.set_active(initial_channel_it);
	initial_channel_appended = true;
}

DABlinGTK::~DABlinGTK() {
	if(eti_source) {
		eti_source->DoExit();
		eti_source_thread.join();
		delete eti_source;
	}

	delete eti_player;

	delete pad_decoder;
	delete fic_decoder;
}

void DABlinGTK::InitWidgets() {
	// init widgets
	frame_combo_channels.set_label("Channel");
	frame_combo_channels.set_size_request(75, -1);
	frame_combo_channels.add(combo_channels);

	combo_channels_liststore = Gtk::ListStore::create(combo_channels_cols);
	combo_channels_liststore->set_sort_column(combo_channels_cols.col_freq, Gtk::SORT_ASCENDING);

	combo_channels.signal_changed().connect(sigc::mem_fun(*this, &DABlinGTK::on_combo_channels));
	combo_channels.set_model(combo_channels_liststore);
	combo_channels.pack_start(combo_channels_cols.col_string);

	if(!options.dab2eti_binary.empty())
		AddChannels();
	else
		frame_combo_channels.set_sensitive(false);

	frame_label_ensemble.set_label("Ensemble");
	frame_label_ensemble.set_size_request(150, -1);
	frame_label_ensemble.add(label_ensemble);
	label_ensemble.set_halign(Gtk::ALIGN_START);
	label_ensemble.set_padding(WIDGET_SPACE, WIDGET_SPACE);


	frame_combo_services.set_label("Service");
	frame_combo_services.set_size_request(170, -1);
	frame_combo_services.add(combo_services);

	combo_services_liststore = Gtk::ListStore::create(combo_services_cols);
	combo_services_liststore->set_sort_column(combo_services_cols.col_sort, Gtk::SORT_ASCENDING);

	combo_services.signal_changed().connect(sigc::mem_fun(*this, &DABlinGTK::on_combo_services));
	combo_services.set_model(combo_services_liststore);
	combo_services.pack_start(combo_services_cols.col_string);


	frame_label_format.set_label("Format");
	frame_label_format.set_size_request(250, -1);
	frame_label_format.set_hexpand(true);
	frame_label_format.add(label_format);
	label_format.set_halign(Gtk::ALIGN_START);
	label_format.set_padding(WIDGET_SPACE, WIDGET_SPACE);

	tglbtn_mute.set_label("Mute");
	tglbtn_mute.signal_clicked().connect(sigc::mem_fun(*this, &DABlinGTK::on_tglbtn_mute));

	frame_label_dl.set_label("Dynamic Label");
	frame_label_dl.set_size_request(750, 50);
	frame_label_dl.set_sensitive(false);
	frame_label_dl.set_hexpand(true);
	frame_label_dl.set_vexpand(true);
	frame_label_dl.add(label_dl);
	label_dl.set_halign(Gtk::ALIGN_START);
	label_dl.set_valign(Gtk::ALIGN_START);
	label_dl.set_padding(WIDGET_SPACE, WIDGET_SPACE);


	top_grid.set_column_spacing(WIDGET_SPACE);
	top_grid.set_row_spacing(WIDGET_SPACE);


	// add widgets
	add(top_grid);

	top_grid.attach(frame_combo_channels, 0, 0, 1, 1);
	top_grid.attach_next_to(frame_label_ensemble, frame_combo_channels, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(frame_combo_services, frame_label_ensemble, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(frame_label_format, frame_combo_services, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(tglbtn_mute, frame_label_format, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach(frame_label_dl, 0, 1, 5, 1);
}

void DABlinGTK::AddChannels() {
	if(options.displayed_channels.empty()) {
		// add all channels
		for(dab_channels_t::const_iterator it = dab_channels.cbegin(); it != dab_channels.cend(); it++)
			AddChannel(it);
	} else {
		// add specific channels
		std::stringstream ss(options.displayed_channels);
		std::string ch;

		while(std::getline(ss, ch, ',')) {
			dab_channels_t::const_iterator it = dab_channels.find(ch);
			if(it != dab_channels.end())
				AddChannel(it);
		}
	}
}

void DABlinGTK::AddChannel(dab_channels_t::const_iterator &it) {
	Gtk::ListStore::iterator row_it = combo_channels_liststore->append();
	Gtk::TreeModel::Row row = *row_it;
	row[combo_channels_cols.col_string] = it->first;
	row[combo_channels_cols.col_freq] = it->second;

	if(it->first == options.initial_channel)
		initial_channel_it = row_it;
}

void DABlinGTK::SetService(SERVICE service) {
	label_format.set_label("");

	frame_label_dl.set_sensitive(false);
	label_dl.set_label("");

	if(service.sid != SERVICE::no_service.sid) {
		char sid_string[7];
		snprintf(sid_string, sizeof(sid_string), "0x%4X", service.sid);

		Glib::ustring label = FICDecoder::ConvertTextToUTF8(service.label.label, 16, service.label.charset);
		set_title(label + " - DABlin");
		frame_combo_services.set_tooltip_text(
				"Short label: \"" + DeriveShortLabel(label, service.label.short_label_mask) + "\"\n"
				"SId: " + sid_string);

		eti_player->SetAudioSubchannel(service.service.subchid, service.service.dab_plus);
	} else {
		set_title("DABlin");
		frame_combo_services.set_tooltip_text("");

		eti_player->SetAudioSubchannel(ETI_PLAYER_NO_SUBCHANNEL, false);
	}
}


void DABlinGTK::on_tglbtn_mute() {
	eti_player->SetAudioMute(tglbtn_mute.get_active());
}


void DABlinGTK::ETIChangeFormatEmitted() {
//	fprintf(stderr, "### ETIChangeFormatEmitted\n");

	label_format.set_label(eti_player->GetFormat());
}

void DABlinGTK::FICChangeEnsembleEmitted() {
//	fprintf(stderr, "### FICChangeEnsembleEmitted\n");

	uint16_t eid;
	FIC_LABEL raw_label;
	fic_decoder->GetEnsembleData(&eid, &raw_label);

	char eid_string[7];
	snprintf(eid_string, sizeof(eid_string), "0x%4X", eid);

	Glib::ustring label = FICDecoder::ConvertTextToUTF8(raw_label.label, 16, raw_label.charset);
	label_ensemble.set_label(label);
	frame_label_ensemble.set_tooltip_text(
			"Short label: \"" + DeriveShortLabel(label, raw_label.short_label_mask) + "\"\n"
			"EId: " + eid_string);
}

void DABlinGTK::FICChangeServicesEmitted() {
//	fprintf(stderr, "### FICChangeServicesEmitted\n");

	services_t new_services = fic_decoder->GetNewServices();

	for(services_t::iterator it = new_services.begin(); it != new_services.end(); it++) {
		Glib::ustring label = FICDecoder::ConvertTextToUTF8(it->label.label, 16, it->label.charset);

//		std::stringstream ss;
//		ss << "'" << label << "' - Subchannel " << it->service.subchid << " " << (it->service.dab_plus ? "(DAB+)" : "(DAB)");

		Gtk::ListStore::iterator row_it = combo_services_liststore->append();
		Gtk::TreeModel::Row row = *row_it;
		row[combo_services_cols.col_sort] = (it->service.subchid << 16) | it->sid;
		row[combo_services_cols.col_string] = label;
		row[combo_services_cols.col_service] = *it;

		if(it->sid == options.initial_sid)
			combo_services.set_active(row_it);
	}
}

void DABlinGTK::on_combo_channels() {
	Gtk::TreeModel::Row row = *combo_channels.get_active();
	uint32_t freq = row[combo_channels_cols.col_freq];

	// cleanup
	if(eti_source) {
		eti_source->DoExit();
		eti_source_thread.join();
		delete eti_source;
	}

	ETIResetFIC();
	combo_services_liststore->clear();	// TODO: prevent on_combo_services() being called for each deleted row
	label_ensemble.set_label("");
	frame_label_ensemble.set_tooltip_text("");
	frame_combo_channels.set_tooltip_text("Center frequency: " + std::to_string(freq) + " kHz");

	// prevent re-use of initial SID
	if(initial_channel_appended)
		options.initial_sid = -1;

	// append
	eti_source = new DAB2ETISource(options.dab2eti_binary, freq, this);
	eti_source_thread = std::thread(&ETISource::Main, eti_source);
}

void DABlinGTK::on_combo_services() {
	Gtk::TreeModel::iterator row_it = combo_services.get_active();
	if(combo_services_liststore->iter_is_valid(row_it)) {
		Gtk::TreeModel::Row row = *row_it;
		SERVICE service = row[combo_services_cols.col_service];
		SetService(service);
	} else {
		SetService(SERVICE::no_service);
	}
}

void DABlinGTK::PADChangeDynamicLabelEmitted() {
//	fprintf(stderr, "### PADChangeDynamicLabelEmitted\n");

	uint8_t dl_raw[DL_MAX_LEN];
	int dl_charset;
	size_t dl_len = pad_decoder->GetDynamicLabel(dl_raw, &dl_charset);

	Glib::ustring label = FICDecoder::ConvertTextToUTF8(dl_raw, dl_len, dl_charset);
	frame_label_dl.set_sensitive(true);
	label_dl.set_label(label);
}

Glib::ustring DABlinGTK::DeriveShortLabel(Glib::ustring long_label, uint16_t short_label_mask) {
	Glib::ustring short_label;

	for(int i = 0; i < 16; i++)
		if(short_label_mask & (0x8000 >> i))
			short_label += long_label[i];

	return short_label;
}
