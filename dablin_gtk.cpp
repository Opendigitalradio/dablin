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
	fprintf(stderr, "Usage: %s [-s <sid>] [file]\n", exe);
	fprintf(stderr, "  -s <sid>      ID of the service to be played (otherwise no service)\n");
	fprintf(stderr, "  file          Input file to be played (stdin, if not specified)\n");
	exit(1);
}


int main(int argc, char **argv) {
	// handle signals
	if(signal(SIGINT, break_handler) == SIG_ERR) {
		perror("DABlin: error while setting SIGINT handler");
		return 1;
	}

	std::string filename;
	int sid = -1;

	// option args
	int c;
	while((c = getopt(argc, argv, "s:")) != -1) {
		switch(c) {
		case 's':
			sid = strtol(optarg, NULL, 0);
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
		filename = std::string(argv[optind]);
		break;
	default:
		usage(argv[0]);
	}


	fprintf(stderr, "DABlin - capital DAB experience\n");


	int myargc = 1;
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(myargc, argv);

	dablin = new DABlinGTK(filename, sid);
	int result = app->run(*dablin);
	delete dablin;

	return result;
}



// --- DABlinGTK -----------------------------------------------------------------
DABlinGTK::DABlinGTK(std::string filename, int initial_sid) {
	this->initial_sid = initial_sid;

	format_change.connect(sigc::mem_fun(*this, &DABlinGTK::ETIChangeFormatEmitted));
	fic_data_change_ensemble.connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeEnsembleEmitted));
	fic_data_change_services.connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeServicesEmitted));

	eti_player = new ETIPlayer(filename, this);
	eti_player_thread = std::thread(&ETIPlayer::Main, eti_player);

	fic_decoder = new FICDecoder(this);

	set_title("DABlin");
	set_default_size(400, 50);
	set_default_icon_name("media-playback-start");

	// init widgets
	frame_label_ensemble.set_label("Ensemble");
	frame_label_ensemble.set_size_request(150, -1);
	frame_label_ensemble.add(label_ensemble);


	combo_services.signal_changed().connect(sigc::mem_fun(*this, &DABlinGTK::on_combo_services));

	frame_combo_services.set_label("Service");
	frame_combo_services.set_size_request(170, -1);
	frame_combo_services.add(combo_services);

	combo_services_liststore = Gtk::ListStore::create(combo_services_cols);
	combo_services_liststore->set_sort_column(combo_services_cols.col_sort, Gtk::SORT_ASCENDING);

	combo_services.set_model(combo_services_liststore);
	combo_services.pack_start(combo_services_cols.col_string);


	frame_label_format.set_label("Format");
	frame_label_format.set_size_request(250, -1);
	frame_label_format.add(label_format);

	chkbtn_mute.set_label("mute");
	chkbtn_mute.signal_clicked().connect(sigc::mem_fun(*this, &DABlinGTK::on_chkbtn_mute));


	top_box.set_spacing(10);


	// add widgets
	add(top_box);

	top_box.pack_start(frame_label_ensemble);
	top_box.pack_start(frame_combo_services);
	top_box.pack_start(frame_label_format);
	top_box.pack_start(chkbtn_mute);

	set_border_width(10);
	show_all_children();
}

DABlinGTK::~DABlinGTK() {
	eti_player->DoExit();

	if(eti_player_thread.joinable())
		eti_player_thread.join();

	delete eti_player;

	delete fic_decoder;
}

void DABlinGTK::SetService(SERVICE service) {
	label_format.set_label("");

	Glib::ustring label = FICDecoder::ConvertTextToUTF8(service.label.label, 16, service.label.charset, false);
	set_title(label + " - DABlin");

	eti_player->SetAudioSubchannel(service.service.subchid, service.service.dab_plus);
}


void DABlinGTK::on_chkbtn_mute() {
	eti_player->SetAudioMute(chkbtn_mute.get_active());
}


void DABlinGTK::ETIChangeFormatEmitted() {
//	fprintf(stderr, "### ETIChangeFormatEmitted\n");

	label_format.set_label(eti_player->GetFormat());
}

void DABlinGTK::FICChangeEnsembleEmitted() {
//	fprintf(stderr, "### FICChangeEnsembleEmitted\n");

	FIC_LABEL raw_label;
	fic_decoder->GetEnsembleData(NULL, &raw_label);

	Glib::ustring label = FICDecoder::ConvertTextToUTF8(raw_label.label, 16, raw_label.charset, false);
	label_ensemble.set_label(label);
}

void DABlinGTK::FICChangeServicesEmitted() {
//	fprintf(stderr, "### FICChangeServicesEmitted\n");

	services_t new_services = fic_decoder->GetNewServices();

	for(services_t::iterator it = new_services.begin(); it != new_services.end(); it++) {
		Glib::ustring label = FICDecoder::ConvertTextToUTF8(it->label.label, 16, it->label.charset, false);

//		std::stringstream ss;
//		ss << "'" << label << "' - Subchannel " << it->service.subchid << " " << (it->service.dab_plus ? "(DAB+)" : "(DAB)");

		Gtk::ListStore::iterator row_it = combo_services_liststore->append();
		Gtk::TreeModel::Row row = *row_it;
		row[combo_services_cols.col_sort] = (it->service.subchid << 16) | it->sid;
		row[combo_services_cols.col_string] = label;
		row[combo_services_cols.col_service] = *it;

		if(it->sid == initial_sid)
			combo_services.set_active(row_it);
	}
}

void DABlinGTK::on_combo_services() {
	Gtk::TreeModel::Row row = *combo_services.get_active();
	SERVICE service = row[combo_services_cols.col_service];

	SetService(service);
}
