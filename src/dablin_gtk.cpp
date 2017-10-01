/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2017 Stefan Pöschel

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

static void break_handler(int) {
	fprintf(stderr, "...DABlin exits...\n");
	if(dablin)
		dablin->hide();
}


static void usage(const char* exe) {
	fprint_dablin_banner(stderr);
	fprintf(stderr, "Usage: %s [OPTIONS] [file]\n", exe);
	fprintf(stderr, "  -h            Show this help\n");
	fprintf(stderr, "  -d <binary>   Use dab2eti as source (using the mentioned binary)\n");
	fprintf(stderr, "  -C <ch>,...   Channels to be displayed (separated by comma; requires dab2eti as source)\n");
	fprintf(stderr, "  -c <ch>       Channel to be played (requires dab2eti as source; otherwise no initial channel)\n");
	fprintf(stderr, "  -s <sid>      ID of the service to be played (otherwise no initial service)\n");
	fprintf(stderr, "  -x <scids>    ID of the service component to be played (requires service ID)\n");
	fprintf(stderr, "  -g <gain>     Set USB stick gain to pass to dab2eti (auto_gain is default)\n");
	fprintf(stderr, "  -p            Output PCM to stdout instead of using SDL\n");
	fprintf(stderr, "  -S            Initially disable slideshow\n");
	fprintf(stderr, "  -L            Enable loose behaviour (e.g. PAD conformance)\n");
	fprintf(stderr, "  file          Input file to be played (stdin, if not specified)\n");
	exit(1);
}


int main(int argc, char **argv) {
	// handle signals
	if(signal(SIGINT, break_handler) == SIG_ERR) {
		perror("DABlin: error while setting SIGINT handler");
		return 1;
	}
	if(signal(SIGTERM, break_handler) == SIG_ERR) {
		perror("DABlin: error while setting SIGTERM handler");
		return 1;
	}

	DABlinGTKOptions options;

	// option args
	int c;
	while((c = getopt(argc, argv, "hd:C:c:g:s:x:pSL")) != -1) {
		switch(c) {
		case 'h':
			usage(argv[0]);
			break;
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
		case 'x':
			options.initial_scids = strtol(optarg, NULL, 0);
			break;
		case 'g':
			options.gain = strtol(optarg, NULL, 0);
			break;
		case 'p':
			options.pcm_output = true;
			break;
		case 'S':
			options.initially_disable_slideshow = true;
			break;
		case 'L':
			options.loose = true;
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
	if(options.initial_scids != LISTED_SERVICE::scids_none && options.initial_sid == LISTED_SERVICE::sid_none) {
		fprintf(stderr, "The service component ID requires the service ID to be specified!\n");
		usage(argv[0]);
	}
#ifdef DABLIN_DISABLE_SDL
	if(!options.pcm_output) {
		fprintf(stderr, "SDL output was disabled, so PCM output must be selected!\n");
		usage(argv[0]);
	}
#endif


	fprint_dablin_banner(stderr);

	int myargc = 1;
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(myargc, argv, "");

	dablin = new DABlinGTK(options);
	int result = app->run(*dablin);
	delete dablin;

	return result;
}



// --- DABlinGTK -----------------------------------------------------------------
DABlinGTK::DABlinGTK(DABlinGTKOptions options) {
	this->options = options;

	initial_channel_appended = false;

	slideshow_window.set_transient_for(*this);

	eti_update_progress.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::ETIUpdateProgressEmitted));
	eti_change_format.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::ETIChangeFormatEmitted));
	fic_change_ensemble.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeEnsembleEmitted));
	fic_change_service.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeServiceEmitted));
	pad_change_dynamic_label.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::PADChangeDynamicLabelEmitted));
	pad_change_slide.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::PADChangeSlideEmitted));

	eti_player = new ETIPlayer(options.pcm_output, this);

	if(!options.dab2eti_binary.empty()) {
		eti_source = NULL;
	} else {
		eti_source = new ETISource(options.filename, this);
		eti_source_thread = std::thread(&ETISource::Main, eti_source);
	}

	fic_decoder = new FICDecoder(this);
	pad_decoder = new PADDecoder(this, options.loose);

	set_title("DABlin v" + std::string(DABLIN_VERSION));
	set_default_icon_name("media-playback-start");

	InitWidgets();

	set_border_width(2 * WIDGET_SPACE);

	// combobox first must be visible
	if(combo_channels_liststore->iter_is_valid(initial_channel_it))
		combo_channels.set_active(initial_channel_it);
	initial_channel_appended = true;

	ConnectKeyPressEventHandler(*this);
	ConnectKeyPressEventHandler(slideshow_window);
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

int DABlinGTK::ComboServicesSlotCompare(const Gtk::TreeModel::iterator& a, const Gtk::TreeModel::iterator& b) {
	const LISTED_SERVICE& service_a = (LISTED_SERVICE) (*a)[combo_services_cols.col_service];
	const LISTED_SERVICE& service_b = (LISTED_SERVICE) (*b)[combo_services_cols.col_service];

	if(service_a < service_b)
		return -1;
	if(service_b < service_a)
		return 1;
	return 0;
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
	combo_services_liststore->set_default_sort_func(sigc::mem_fun(*this, &DABlinGTK::ComboServicesSlotCompare));
	combo_services_liststore->set_sort_column(Gtk::TreeSortable::DEFAULT_SORT_COLUMN_ID, Gtk::SORT_ASCENDING);

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

	vlmbtn.set_value(1.0);
	vlmbtn.set_sensitive(eti_player->HasAudioVolumeControl());
	vlmbtn.signal_value_changed().connect(sigc::mem_fun(*this, &DABlinGTK::on_vlmbtn));

	tglbtn_slideshow.set_label("Slideshow");
	tglbtn_slideshow.set_active(!options.initially_disable_slideshow);
	tglbtn_slideshow.signal_clicked().connect(sigc::mem_fun(*this, &DABlinGTK::on_tglbtn_slideshow));

	frame_label_dl.set_label("Dynamic Label");
	frame_label_dl.set_size_request(750, 50);
	frame_label_dl.set_sensitive(false);
	frame_label_dl.set_hexpand(true);
	frame_label_dl.set_vexpand(true);
	frame_label_dl.add(label_dl);
	label_dl.set_halign(Gtk::ALIGN_START);
	label_dl.set_valign(Gtk::ALIGN_START);
	label_dl.set_padding(WIDGET_SPACE, WIDGET_SPACE);

	progress_position.set_show_text();


	top_grid.set_column_spacing(WIDGET_SPACE);
	top_grid.set_row_spacing(WIDGET_SPACE);


	// add widgets
	add(top_grid);

	top_grid.attach(frame_combo_channels, 0, 0, 1, 2);
	top_grid.attach_next_to(frame_label_ensemble, frame_combo_channels, Gtk::POS_RIGHT, 1, 2);
	top_grid.attach_next_to(frame_combo_services, frame_label_ensemble, Gtk::POS_RIGHT, 1, 2);
	top_grid.attach_next_to(frame_label_format, frame_combo_services, Gtk::POS_RIGHT, 1, 2);
	top_grid.attach_next_to(tglbtn_mute, frame_label_format, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(vlmbtn, tglbtn_mute, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(tglbtn_slideshow, tglbtn_mute, Gtk::POS_BOTTOM, 2, 1);
	top_grid.attach_next_to(frame_label_dl, frame_combo_channels, Gtk::POS_BOTTOM, 6, 1);
	top_grid.attach_next_to(progress_position, frame_label_dl, Gtk::POS_BOTTOM, 6, 1);

	show_all_children();
	progress_position.hide();	// invisible until progress updated
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
			if(it != dab_channels.cend())
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

void DABlinGTK::SetService(const LISTED_SERVICE& service) {
	if(!service.IsNone()) {
		char sid_string[7];
		snprintf(sid_string, sizeof(sid_string), "0x%04X", service.sid);

		Glib::ustring label = FICDecoder::ConvertLabelToUTF8(service.label);
		set_title(label + " - DABlin");
		frame_combo_services.set_tooltip_text(
				"Short label: \"" + DeriveShortLabel(label, service.label.short_label_mask) + "\"\n"
				"SId: " + sid_string + (!service.IsPrimary() ? " (SCIdS: " + std::to_string(service.scids) + ")" : "") + "\n"
				"SubChId: " + std::to_string(service.audio_service.subchid) + "\n"
				"Audio type: " + (service.audio_service.dab_plus ? "DAB+" : "DAB")
		);
	} else {
		set_title("DABlin");
		frame_combo_services.set_tooltip_text("");
	}

	// if the audio service changed, reset format/DL/slide + switch
	if(!eti_player->IsSameAudioService(service.audio_service)) {
		label_format.set_label("");

		frame_label_dl.set_sensitive(false);
		label_dl.set_label("");

		slideshow_window.hide();
		slideshow_window.ClearSlide();

		eti_player->SetAudioService(service.audio_service);
	}
}


void DABlinGTK::on_tglbtn_mute() {
	eti_player->SetAudioMute(tglbtn_mute.get_active());
}

void DABlinGTK::on_vlmbtn(double value) {
	eti_player->SetAudioVolume(value);

	// disable mute, if needed
	if(tglbtn_mute.get_active())
		tglbtn_mute.clicked();
}

void DABlinGTK::on_tglbtn_slideshow() {
	if(tglbtn_slideshow.get_active())
		slideshow_window.TryToShow();
	else
		slideshow_window.hide();
}

void DABlinGTK::ConnectKeyPressEventHandler(Gtk::Widget& widget) {
	widget.signal_key_press_event().connect(sigc::mem_fun(*this, &DABlinGTK::HandleKeyPressEvent));
	widget.add_events(Gdk::KEY_PRESS_MASK);
}

bool DABlinGTK::HandleKeyPressEvent(GdkEventKey* key_event) {
	// consider only events without Shift/Control/Alt
	if((key_event->state & (Gdk::SHIFT_MASK | Gdk::CONTROL_MASK | Gdk::MOD1_MASK)) == 0) {
		switch(key_event->keyval) {
		case GDK_KEY_m:
		case GDK_KEY_M:
			// toggle mute
			tglbtn_mute.clicked();
			return true;
		}
	}
	return false;
}

void DABlinGTK::ETIUpdateProgressEmitted() {
//	fprintf(stderr, "### ETIUpdateProgressEmitted\n");

	ETI_PROGRESS progress = eti_update_progress.Pop();

	progress_position.set_fraction(progress.value);
	progress_position.set_text(progress.text);
	if(!progress_position.get_visible())
		progress_position.show();
}

void DABlinGTK::ETIChangeFormatEmitted() {
//	fprintf(stderr, "### ETIChangeFormatEmitted\n");

	label_format.set_label(eti_change_format.Pop());
}

void DABlinGTK::FICChangeEnsembleEmitted() {
//	fprintf(stderr, "### FICChangeEnsembleEmitted\n");

	FIC_ENSEMBLE new_ensemble = fic_change_ensemble.Pop();

	char eid_string[7];
	snprintf(eid_string, sizeof(eid_string), "0x%04X", new_ensemble.eid);

	Glib::ustring label = FICDecoder::ConvertLabelToUTF8(new_ensemble.label);
	label_ensemble.set_label(label);
	frame_label_ensemble.set_tooltip_text(
			"Short label: \"" + DeriveShortLabel(label, new_ensemble.label.short_label_mask) + "\"\n"
			"EId: " + eid_string);
}

void DABlinGTK::FICChangeServiceEmitted() {
//	fprintf(stderr, "### FICChangeServiceEmitted\n");

	LISTED_SERVICE new_service = fic_change_service.Pop();

	Glib::ustring label = FICDecoder::ConvertLabelToUTF8(new_service.label);
	if(new_service.multi_comps)
		label = (!new_service.IsPrimary() ? "» " : "") + label + (new_service.IsPrimary() ? " »" : "");

	// get row (add new one, if needed)
	Gtk::ListStore::Children children = combo_services_liststore->children();
	Gtk::ListStore::iterator row_it = std::find_if(
			children.begin(), children.end(),
			[&](const Gtk::TreeModel::Row& row)->bool {
				const LISTED_SERVICE& ls = (LISTED_SERVICE) row[combo_services_cols.col_service];
				return ls.sid == new_service.sid && ls.scids == new_service.scids;
			}
	);
	bool add_new_row = row_it == children.end();
	if(add_new_row)
		row_it = combo_services_liststore->append();

	Gtk::TreeModel::Row row = *row_it;
	row[combo_services_cols.col_string] = label;
	row[combo_services_cols.col_service] = new_service;

	if(add_new_row) {
		// set (initial) service
		if(new_service.sid == options.initial_sid && new_service.scids == options.initial_scids)
			combo_services.set_active(row_it);
	} else {
		// set (updated) service
		Gtk::ListStore::iterator current_it = combo_services.get_active();
		if(current_it && current_it == row_it)
			SetService(new_service);
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
	if(initial_channel_appended) {
		options.initial_sid = LISTED_SERVICE::sid_none;
		options.initial_scids = LISTED_SERVICE::scids_none;
	}

	// append
	eti_source = new DAB2ETISource(options.dab2eti_binary, freq, options.gain, this);
	eti_source_thread = std::thread(&ETISource::Main, eti_source);
}

void DABlinGTK::on_combo_services() {
	LISTED_SERVICE service;	// default: none
	Gtk::TreeModel::iterator row_it = combo_services.get_active();
	if(combo_services_liststore->iter_is_valid(row_it)) {
		Gtk::TreeModel::Row row = *row_it;
		service = row[combo_services_cols.col_service];
	}
	SetService(service);
}

void DABlinGTK::PADChangeDynamicLabelEmitted() {
//	fprintf(stderr, "### PADChangeDynamicLabelEmitted\n");

	DL_STATE dl = pad_change_dynamic_label.Pop();
	Glib::ustring label = FICDecoder::ConvertTextToUTF8(&dl.raw[0], dl.raw.size(), dl.charset);
	frame_label_dl.set_sensitive(true);
	label_dl.set_label(label);
}

void DABlinGTK::PADChangeSlideEmitted() {
//	fprintf(stderr, "### PADChangeSlideEmitted\n");

	slideshow_window.UpdateSlide(pad_change_slide.Pop());
	if(tglbtn_slideshow.get_active())
		slideshow_window.TryToShow();
}


Glib::ustring DABlinGTK::DeriveShortLabel(Glib::ustring long_label, uint16_t short_label_mask) {
	Glib::ustring short_label;

	for(int i = 0; i < 16; i++)
		if(short_label_mask & (0x8000 >> i))
			short_label += long_label[i];

	return short_label;
}


// --- DABlinGTKSlideshowWindow -----------------------------------------------------------------
DABlinGTKSlideshowWindow::DABlinGTKSlideshowWindow() {
	set_title("Slideshow");
	set_type_hint(Gdk::WINDOW_TYPE_HINT_UTILITY);
	set_resizable(false);
	set_deletable(false);

	add(top_grid);

	top_grid.attach(image, 0, 0, 1, 1);
	top_grid.attach_next_to(link_button, image, Gtk::POS_BOTTOM, 1, 1);

	show_all_children();
}

void DABlinGTKSlideshowWindow::TryToShow() {
	// if already visible or no slide, abort
	if(get_visible() || image.get_storage_type() == Gtk::ImageType::IMAGE_EMPTY)
		return;

	// arrange to the right of parent
	int x, y, w, h;
	get_transient_for()->get_position(x, y);
	get_transient_for()->get_size(w, h);
	move(x + w + 20, y);	// add some horizontal padding for WM decoration

	show();
}

void DABlinGTKSlideshowWindow::UpdateSlide(const MOT_FILE& slide) {
	std::string type_mime = "";
	std::string type_display = "unknown";

	switch(slide.content_sub_type) {
	case 0x01:	// JFIF
		type_mime = "image/jpeg";
		type_display = "JPEG";
		break;
	case 0x03:	// PNG
		type_mime = "image/png";
		type_display = "PNG";
		break;
	}

	Glib::RefPtr<Gdk::PixbufLoader> pixbuf_loader = Gdk::PixbufLoader::create(type_mime, true);
	pixbuf_loader->write(&slide.data[0], slide.data.size());
	pixbuf_loader->close();

	Glib::RefPtr<Gdk::Pixbuf> pixbuf = pixbuf_loader->get_pixbuf();
	if(!pixbuf)
		return;

	// update title
	std::string title = "Slideshow";
	if(!slide.category_title.empty())
		title = slide.category_title + " - " + title;
	set_title(title);

	// update image
	image.set(pixbuf);
	image.set_tooltip_text(
			"Resolution: " + std::to_string(pixbuf->get_width()) + "x" + std::to_string(pixbuf->get_height()) + " pixels\n" +
			"Size: " + std::to_string(slide.data.size()) + " bytes\n" +
			"Format: " + type_display + "\n" +
			"Content name: " + slide.content_name);

	// update ClickThroughURL link
	if(!slide.click_through_url.empty()) {
		link_button.set_label(slide.click_through_url);
		link_button.set_tooltip_text(slide.click_through_url);
		link_button.set_uri(slide.click_through_url);

		// ensure that the label (created/replaced by set_label) does not extend the slide
		Gtk::Label* l = (Gtk::Label*) link_button.get_child();
		l->set_max_width_chars(1);
		l->set_ellipsize(Pango::ELLIPSIZE_END);

		link_button.show();
	} else {
		link_button.hide();
	}
}
