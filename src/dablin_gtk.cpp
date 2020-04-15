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

#include "dablin_gtk.h"

static DABlinGTK *dablin = nullptr;

static void break_handler(int) {
	fprintf(stderr, "...DABlin exits...\n");
	if(dablin)
		dablin->hide();
}


static void usage(const char* exe) {
	DABlinGTKOptions options_default;

	fprint_dablin_banner(stderr);
	fprintf(stderr, "Usage: %s [OPTIONS] [file]\n", exe);
	fprintf(stderr, "  -h           Show this help\n"
					"  -f <format>  Source format: \"%s\" (default), \"%s\"\n"
					"  -d <binary>  Use DAB live source (using the mentioned binary)\n"
					"  -D <type>    DAB live source type: \"%s\" (default), \"%s\"\n"
					"  -C <ch>,...  Channels to be listed (comma separated; requires DAB live source;\n"
					"               an optional gain can also be specified, e.g. \"5C:-54\")\n"
					"  -c <ch>      Channel to be played (requires DAB live source)\n"
					"  -l <label>   Label of the service to be played\n"
					"  -1           Play the first service found\n"
					"  -s <sid>     ID of the service to be played\n"
					"  -x <scids>   ID of the service component to be played (requires service ID)\n"
					"  -g <gain>    USB stick gain to pass to DAB live source (auto gain is default)\n"
					"  -G           Use default gain for DAB live source (instead of auto gain)\n"
					"  -r <path>    Path for recordings (default: %s)\n"
					"  -P <size>    Recording prebuffer size in seconds (default: %zu)\n"
					"  -p           Output PCM to stdout instead of using SDL\n"
					"  -u           Output untouched audio stream to stdout instead of using SDL\n"
					"  -I           Don't catch up on stream after interruption\n"
					"  -S           Initially disable slideshow\n"
					"  -L           Enable loose behaviour (e.g. PAD conformance)\n"
					"  -F           Disable dynamic FIC messages (dynamic PTY, announcements)\n"
					"  file         Input file to be played (stdin, if not specified)\n",
					EnsembleSource::FORMAT_ETI.c_str(),
					EnsembleSource::FORMAT_EDI.c_str(),
					DABLiveETISource::TYPE_DAB2ETI.c_str(),
					DABLiveETISource::TYPE_ETI_CMDLINE.c_str(),
					options_default.recordings_path.c_str(),
					options_default.rec_prebuffer_size_s
			);
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
	int initial_param_count = 0;
	int gain_param_count = 0;

	// option args
	int c;
	while((c = getopt(argc, argv, "hf:d:D:C:c:l:g:Gr:P:s:x:1puISLF")) != -1) {
		switch(c) {
		case 'h':
			usage(argv[0]);
			break;
		case 'f':
			options.source_format = optarg;
			break;
		case 'd':
			options.dab_live_source_binary = optarg;
			break;
		case 'D':
			options.dab_live_source_type = optarg;
			break;
		case 'C':
			options.displayed_channels = optarg;
			break;
		case 'c':
			options.initial_channel = optarg;
			break;
		case 'l':
			options.initial_label = optarg;
			initial_param_count++;
			break;
		case '1':
			options.initial_first_found_service = true;
			initial_param_count++;
			break;
		case 's':
			options.initial_sid = strtol(optarg, nullptr, 0);
			initial_param_count++;
			break;
		case 'x':
			options.initial_scids = strtol(optarg, nullptr, 0);
			break;
		case 'g':
			options.gain = strtol(optarg, nullptr, 0);
			gain_param_count++;
			break;
		case 'G':
			options.gain = DAB_LIVE_SOURCE_CHANNEL::default_gain;
			gain_param_count++;
			break;
		case 'r':
			options.recordings_path = optarg;
			break;
		case 'P':
			options.rec_prebuffer_size_s = strtol(optarg, nullptr, 0);
			break;
		case 'p':
			options.pcm_output = true;
			break;
		case 'u':
			options.untouched_output = true;
			break;
		case 'I':
			options.disable_int_catch_up = true;
			break;
		case 'S':
			options.initially_disable_slideshow = true;
			break;
		case 'L':
			options.loose = true;
			break;
		case 'F':
			options.disable_dyn_fic_msgs = true;
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
	if(options.dab_live_source_binary.empty()) {
		if(!options.displayed_channels.empty()) {
			fprintf(stderr, "If displayed channels are selected, DAB live source must be used!\n");
			usage(argv[0]);
		}
		if(!options.initial_channel.empty()) {
			fprintf(stderr, "If a channel is selected, DAB live source must be used!\n");
			usage(argv[0]);
		}
	} else {
		if(options.source_format != EnsembleSource::FORMAT_ETI) {
			fprintf(stderr, "A DAB live source can only be used with ETI source format!\n");
			usage(argv[0]);
		}
		if(!options.filename.empty()) {
			fprintf(stderr, "Both a file and DAB live source cannot be used as source!\n");
			usage(argv[0]);
		}
		if(!options.initial_channel.empty() && dab_channels.find(options.initial_channel) == dab_channels.end()) {
			fprintf(stderr, "The channel '%s' is not supported!\n", options.initial_channel.c_str());
			usage(argv[0]);
		}
		if(options.dab_live_source_type != DABLiveETISource::TYPE_DAB2ETI && options.dab_live_source_type != DABLiveETISource::TYPE_ETI_CMDLINE) {
			fprintf(stderr, "The DAB live source type '%s' is not supported!\n", options.dab_live_source_type.c_str());
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

	if(options.pcm_output && options.untouched_output) {
		fprintf(stderr, "No more than one output option can be specified!\n");
		usage(argv[0]);
	}


	// at most one param needed!
	if(initial_param_count > 1) {
		fprintf(stderr, "At most one initial parameter shall be specified!\n");
		usage(argv[0]);
	}
	if(gain_param_count > 1) {
		fprintf(stderr, "At most one gain parameter shall be specified!\n");
		usage(argv[0]);
	}


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

	switch_service_label = options.initial_label;
	switch_service_sid = options.initial_sid;
	switch_service_scids = options.initial_scids;
	switch_service_applied = false;

	rec_file = nullptr;
	rec_duration_ms = 0;
	rec_prebuffer_filled_ms = 0;

	dt_lto = FIC_ENSEMBLE::lto_none;

	slideshow_window.set_transient_for(*this);

	ensemble_update_progress.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::EnsembleUpdateProgressEmitted));
	ensemble_change_format.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::EnsembleChangeFormatEmitted));
	fic_change_ensemble.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeEnsembleEmitted));
	fic_change_service.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeServiceEmitted));
	pad_change_dynamic_label.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::PADChangeDynamicLabelEmitted));
	pad_change_slide.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::PADChangeSlideEmitted));
	do_rec_status_update.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::DoRecStatusUpdateEmitted));
	do_datetime_update.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::DoDateTimeUpdateEmitted));

	if(options.source_format == EnsembleSource::FORMAT_ETI)
		ensemble_player = new ETIPlayer(options.pcm_output, options.untouched_output, options.disable_int_catch_up, this);
	else
		ensemble_player = new EDIPlayer(options.pcm_output, options.untouched_output, options.disable_int_catch_up, this);

	if(options.source_format == EnsembleSource::FORMAT_ETI) {
		if(!options.dab_live_source_binary.empty()) {
			ensemble_source = nullptr;
		} else {
			ensemble_source = new ETISource(options.filename, this);
			ensemble_source_thread = std::thread(&EnsembleSource::Main, ensemble_source);
		}
	} else {
		ensemble_source = new EDISource(options.filename, this);
		ensemble_source_thread = std::thread(&EnsembleSource::Main, ensemble_source);
	}

	fic_decoder = new FICDecoder(this, options.disable_dyn_fic_msgs);
	pad_decoder = new PADDecoder(this, options.loose);

	set_title("DABlin v" + std::string(DABLIN_VERSION));
	set_icon_name("media-playback-stop");

	InitWidgets();

	set_border_width(2 * WIDGET_SPACE);

	// combobox first must be visible
	if(combo_channels_liststore->iter_is_valid(initial_channel_it))
		combo_channels.set_active(initial_channel_it);

	// add window key press event handler
	signal_key_press_event().connect(sigc::mem_fun(*this, &DABlinGTK::HandleKeyPressEvent));
	add_events(Gdk::KEY_PRESS_MASK);

	// add window config event handler (before default handler)
	signal_configure_event().connect(sigc::mem_fun(*this, &DABlinGTK::HandleConfigureEvent), false);
	add_events(Gdk::STRUCTURE_MASK);
}

DABlinGTK::~DABlinGTK() {
	if(ensemble_source) {
		ensemble_source->DoExit();
		ensemble_source_thread.join();
		delete ensemble_source;
	}

	delete ensemble_player;

	delete pad_decoder;
	delete fic_decoder;
}

int DABlinGTK::ComboChannelsSlotCompare(const Gtk::TreeModel::iterator& a, const Gtk::TreeModel::iterator& b) {
	const DAB_LIVE_SOURCE_CHANNEL& ch_a = (DAB_LIVE_SOURCE_CHANNEL) (*a)[combo_channels_cols.col_channel];
	const DAB_LIVE_SOURCE_CHANNEL& ch_b = (DAB_LIVE_SOURCE_CHANNEL) (*b)[combo_channels_cols.col_channel];

	if(ch_a < ch_b)
		return -1;
	if(ch_b < ch_a)
		return 1;
	return 0;
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
	this->signal_delete_event().connect(sigc::mem_fun(*this, &DABlinGTK::on_window_delete_event));

	// init widgets
	frame_combo_channels.set_label("Channel");
	frame_combo_channels.set_size_request(100, -1);
	frame_combo_channels.add(channels_box);

	channels_box.pack_start(combo_channels);
	channels_box.pack_start(btn_channels_stop, Gtk::PACK_SHRINK);

	combo_channels_liststore = Gtk::ListStore::create(combo_channels_cols);
	combo_channels_liststore->set_default_sort_func(sigc::mem_fun(*this, &DABlinGTK::ComboChannelsSlotCompare));
	combo_channels_liststore->set_sort_column(Gtk::TreeSortable::DEFAULT_SORT_COLUMN_ID, Gtk::SORT_ASCENDING);

	combo_channels.signal_changed().connect(sigc::mem_fun(*this, &DABlinGTK::on_combo_channels));
	combo_channels.set_model(combo_channels_liststore);
	combo_channels.pack_start(combo_channels_cols.col_string);

	if(!options.dab_live_source_binary.empty())
		AddChannels();
	else
		frame_combo_channels.set_sensitive(false);

	btn_channels_stop.set_image_from_icon_name("media-playback-start");
	btn_channels_stop.signal_clicked().connect(sigc::mem_fun(*this, &DABlinGTK::on_btn_channels_stop));
	btn_channels_stop.set_sensitive(false);
	btn_channels_stop.set_tooltip_text("Stop/resume decoding current channel/service");

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

	tglbtn_record.set_image_from_icon_name("media-record");
	tglbtn_record.signal_clicked().connect(sigc::mem_fun(*this, &DABlinGTK::on_tglbtn_record));
	tglbtn_record.set_sensitive(false);
	tglbtn_record.set_tooltip_text("Record service");

	label_record.set_halign(Gtk::ALIGN_START);
	label_record.set_padding(WIDGET_SPACE, WIDGET_SPACE);

	tglbtn_slideshow.set_image_from_icon_name("video-display");
	tglbtn_slideshow.set_active(!options.initially_disable_slideshow);
	tglbtn_slideshow.signal_clicked().connect(sigc::mem_fun(*this, &DABlinGTK::on_tglbtn_slideshow));
	tglbtn_slideshow.set_tooltip_text("Slideshow (if available)");

	tglbtn_mute.set_image_from_icon_name("audio-volume-muted");
	tglbtn_mute.signal_clicked().connect(sigc::mem_fun(*this, &DABlinGTK::on_tglbtn_mute));
	tglbtn_mute.set_tooltip_text("Mute");

	vlmbtn.set_value(1.0);
	vlmbtn.set_sensitive(ensemble_player->HasAudioVolumeControl());
	vlmbtn.signal_value_changed().connect(sigc::mem_fun(*this, &DABlinGTK::on_vlmbtn));

	frame_label_dl.set_label("Dynamic Label");
	frame_label_dl.set_size_request(750, 50);
	frame_label_dl.set_sensitive(false);
	frame_label_dl.set_hexpand(true);
	frame_label_dl.set_vexpand(true);
	frame_label_dl.add(label_dl);
	label_dl.set_halign(Gtk::ALIGN_START);
	label_dl.set_valign(Gtk::ALIGN_START);
	label_dl.set_padding(WIDGET_SPACE, WIDGET_SPACE);

	frame_label_datetime.set_label("Local date/time");
	frame_label_datetime.set_sensitive(false);
	frame_label_datetime.add(label_datetime);
	label_datetime.set_halign(Gtk::ALIGN_START);
	label_datetime.set_padding(WIDGET_SPACE, WIDGET_SPACE);

	frame_label_asu.set_label("Announcement support");
	frame_label_asu.set_sensitive(false);
	frame_label_asu.set_hexpand(true);
	frame_label_asu.add(label_asu);
	label_asu.set_halign(Gtk::ALIGN_START);
	label_asu.set_padding(WIDGET_SPACE, WIDGET_SPACE);

	progress_position.set_show_text();


	top_grid.set_column_spacing(WIDGET_SPACE);
	top_grid.set_row_spacing(WIDGET_SPACE);


	// add widgets
	add(top_grid);

	top_grid.attach(frame_combo_channels, 0, 0, 1, 2);
	top_grid.attach_next_to(frame_label_ensemble, frame_combo_channels, Gtk::POS_RIGHT, 1, 2);
	top_grid.attach_next_to(frame_combo_services, frame_label_ensemble, Gtk::POS_RIGHT, 1, 2);
	top_grid.attach_next_to(frame_label_format, frame_combo_services, Gtk::POS_RIGHT, 1, 2);
	top_grid.attach_next_to(tglbtn_record, frame_label_format, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(label_record, tglbtn_record, Gtk::POS_RIGHT, 2, 1);
	top_grid.attach_next_to(tglbtn_slideshow, tglbtn_record, Gtk::POS_BOTTOM, 1, 1);
	top_grid.attach_next_to(tglbtn_mute, tglbtn_slideshow, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(vlmbtn, tglbtn_mute, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(frame_label_dl, frame_combo_channels, Gtk::POS_BOTTOM, 7, 1);
	top_grid.attach_next_to(frame_label_datetime, frame_label_dl, Gtk::POS_BOTTOM, 2, 1);
	top_grid.attach_next_to(frame_label_asu, frame_label_datetime, Gtk::POS_RIGHT, 5, 1);
	top_grid.attach_next_to(progress_position, frame_label_datetime, Gtk::POS_BOTTOM, 7, 1);

	show_all_children();
	progress_position.hide();	// invisible until progress updated
}

void DABlinGTK::AddChannels() {
	if(options.displayed_channels.empty()) {
		// add all channels
		for(const dab_channels_t::value_type& dab_channel : dab_channels)
			AddChannel(dab_channel, options.gain);
	} else {
		// add specific channels
		string_vector_t channels = StringTools::SplitString(options.displayed_channels, ',');
		for(const std::string& channel : channels) {
			int gain = options.gain;
			string_vector_t parts = StringTools::SplitString(channel, ':');
			switch(parts.size()) {
			case 2:
				gain = strtol(parts[1].c_str(), nullptr, 0);
				// fall through
			case 1: {
				dab_channels_t::const_iterator it = dab_channels.find(parts[0]);
				if(it != dab_channels.cend())
					AddChannel(*it, gain);
				else
					fprintf(stderr, "DABlinGTK: The channel '%s' is not supported; ignoring!\n", parts[0].c_str());
				break; }
			default:
				fprintf(stderr, "DABlinGTK: The format of channel '%s' is not supported; ignoring!\n", channel.c_str());
				continue;
			}
		}
	}
}

void DABlinGTK::AddChannel(const dab_channels_t::value_type& dab_channel, int gain) {
	Gtk::ListStore::iterator row_it = combo_channels_liststore->append();
	Gtk::TreeModel::Row row = *row_it;
	row[combo_channels_cols.col_string] = dab_channel.first;
	row[combo_channels_cols.col_channel] = DAB_LIVE_SOURCE_CHANNEL(dab_channel.first, dab_channel.second, gain);

	if(dab_channel.first == options.initial_channel)
		initial_channel_it = row_it;
}

void DABlinGTK::SetService(const LISTED_SERVICE& service) {
	// (re)set labels/tooltips/record button
	if(!service.IsNone()) {
		std::string charset_name;
		std::string label = FICDecoder::ConvertLabelToUTF8(service.label, &charset_name);

		set_title(label + " - DABlin");
		set_icon_name(tglbtn_record.get_active() ? "media-record" : "media-playback-start");

		std::string tooltip_text =
				"Short label: \"" + FICDecoder::DeriveShortLabelUTF8(label, service.label.short_label_mask) + "\"\n"
				"Label charset: " + charset_name + "\n"
				"SId: " + StringTools::IntToHex(service.sid, 4) + (!service.IsPrimary() ? " (SCIdS: " + std::to_string(service.scids) + ")" : "") + "\n"
				"SubChId: " + std::to_string(service.audio_service.subchid) + "\n"
				"Audio type: " + (service.audio_service.dab_plus ? "DAB+" : "DAB");
		if(ensemble.inter_table_id != FIC_ENSEMBLE::inter_table_id_none) {
			if(service.pty_static != LISTED_SERVICE::pty_none)
				tooltip_text += "\n" "Programme type (static): " + FICDecoder::ConvertPTYToString(service.pty_static, ensemble.inter_table_id);
			if(service.pty_dynamic != LISTED_SERVICE::pty_none)
				tooltip_text += "\n" "Programme type (dynamic): " + FICDecoder::ConvertPTYToString(service.pty_dynamic, ensemble.inter_table_id);
		}
		frame_combo_services.set_tooltip_text(tooltip_text);

		if(!service.subchannel.IsNone()) {
			tooltip_text = "";
			if(!service.subchannel.pl.empty()) {
				tooltip_text +=
						"Sub-channel start: " + std::to_string(service.subchannel.start) + " CUs\n"
						"Sub-channel size: " + std::to_string(service.subchannel.size) + " CUs\n"
						"Protection level: " + service.subchannel.pl + "\n"
						"Bit rate: " + std::to_string(service.subchannel.bitrate) + " kBit/s";
			}
			if(service.subchannel.language != FIC_SUBCHANNEL::language_none) {
				if(!tooltip_text.empty())
					tooltip_text += "\n";
				tooltip_text += "Language: " + FICDecoder::ConvertLanguageToString(service.subchannel.language);
			}
			frame_label_format.set_tooltip_text(tooltip_text);
		}

		tglbtn_record.set_sensitive(true);
	} else {
		set_title("DABlin");
		set_icon_name("media-playback-stop");

		frame_combo_services.set_tooltip_text("");
		frame_label_format.set_tooltip_text("");

		tglbtn_record.set_sensitive(false);
	}

	UpdateAnnouncementSupport(service);

	// if the audio service changed, reset format/DL/slide + switch
	bool audio_service_changed = !ensemble_player->IsSameAudioService(service.audio_service);
	if(audio_service_changed) {
		// stop playback of old service
		if(options.rec_prebuffer_size_s > 0)
			ensemble_player->RemoveUntouchedStreamConsumer(this);
		ensemble_player->SetAudioService(AUDIO_SERVICE());

		pad_decoder->Reset();

		label_format.set_label("");

		frame_label_dl.set_sensitive(false);
		label_dl.set_label("");
		frame_label_dl.set_tooltip_text("");

		if(!service.HasSLS()) {
			slideshow_window.hide();
			slideshow_window.ClearSlide();
		}

		{
			std::lock_guard<std::mutex> lock(rec_mutex);

			// clear prebuffer, if enabled
			if(options.rec_prebuffer_size_s > 0) {
				rec_prebuffer.clear();
				rec_prebuffer_filled_ms = 0;
			}

			UpdateRecStatus(!service.IsNone());
		}

		// start playback of new service
		ensemble_player->SetAudioService(service.audio_service);
		if(options.rec_prebuffer_size_s > 0)
			ensemble_player->AddUntouchedStreamConsumer(this);
	}

	// (re)init SLS on new service (having SLS) only once
	if(service.HasSLS() && (audio_service_changed || slideshow_window.IsEmptySlide())) {
		slideshow_window.AwaitSlide();
		if(tglbtn_slideshow.get_active())
			slideshow_window.TryToShow();
		pad_decoder->SetMOTAppType(service.sls_app_type);
	}
}

void DABlinGTK::UpdateAnnouncementSupport(const LISTED_SERVICE& service) {
	// reset, if no service/announcement support
	if(service.IsNone() || service.asu_flags == LISTED_SERVICE::asu_flags_none) {
		frame_label_asu.set_sensitive(false);
		frame_label_asu.set_tooltip_markup("");
		label_asu.set_label("");
		return;
	}

	// assemble text - iterate through all supported announcement types
	std::string ac_str;
	for(int type = 0; type < 16; type++) {
		uint16_t type_flag = 1 << type;
		if(!(service.asu_flags & type_flag))
			continue;

		// check all matching announcement clusters for active announcement
		std::string asw_color;
		for(const cids_t::value_type& cid : service.cids) {
			asw_clusters_t::const_iterator it = ensemble.asw_clusters.find(cid);
			if(it == ensemble.asw_clusters.cend() || !(it->second.asw_flags & type_flag))
				continue;

			if(it->second.subchid == service.audio_service.subchid) {
				asw_color = "yellow";
			} else {
				if(asw_color.empty())
					asw_color = "cyan";
			}
		}

		if(!ac_str.empty())
			ac_str += " + ";
		if(!asw_color.empty())
			ac_str += "<span bgcolor='" + asw_color + "'>";
		ac_str += FICDecoder::ConvertASuTypeToString(type);
		if(!asw_color.empty())
			ac_str += "</span>";
	}

	// assemble tooltip - iterate through all matching announcement clusters
	std::string ac_tooltip;
	char cid_string[5];
	for(const cids_t::value_type& cid : service.cids) {
		asw_clusters_t::const_iterator it = ensemble.asw_clusters.find(cid);
		bool present = it != ensemble.asw_clusters.cend();
		bool active = present && (service.asu_flags & it->second.asw_flags);

		ac_tooltip += ac_tooltip.empty() ? "Cluster(s): " : " / ";
		if(active)
			ac_tooltip += "<u>";
		snprintf(cid_string, sizeof(cid_string), "0x%02X", cid);
		ac_tooltip += std::string(cid_string);
		if(present)
			ac_tooltip += " (SubChId " + std::to_string(it->second.subchid) + ")";
		if(active)
			ac_tooltip += "</u>";
	}

	frame_label_asu.set_sensitive(true);
	frame_label_asu.set_tooltip_markup(ac_tooltip);
	label_asu.set_markup(ac_str);
}

void DABlinGTK::on_tglbtn_record() {
	if(tglbtn_record.get_active()) {
		// get start timestamp
		time_t start;
		{
			std::lock_guard<std::mutex> lock(rec_mutex);

			// use prebuffer start information, if available
			if(!rec_prebuffer.empty()) {
				start = rec_prebuffer.front().ts;
			} else {
				start = time(nullptr);
				if(start == (time_t) -1)
					perror("DABlinGTK: error while getting time for recording");
			}
		}
		struct tm* start_tm = localtime(&start);
		if(!start_tm)
			perror("DABlinGTK: error while getting local time");
		char start_string[22];
		strftime(start_string, sizeof(start_string), "%F - %H-%M-%S", start_tm);

		// get station name
		LISTED_SERVICE service;	// default: none
		Gtk::TreeModel::iterator row_it = combo_services.get_active();
		if(combo_services_liststore->iter_is_valid(row_it)) {
			Gtk::TreeModel::Row row = *row_it;
			service = row[combo_services_cols.col_service];
		}
		std::string label = FICDecoder::ConvertLabelToUTF8(service.label, nullptr);

		// escape forbidden '/' character
		std::string label_cleaned = label;
		for(char& c : label_cleaned)
			if(c == '/')
				c = '_';

		std::string new_rec_filename = options.recordings_path + "/" + std::string(start_string) + " - " + label_cleaned + "." + ensemble_player->GetUntouchedStreamFileExtension();
		FILE* new_rec_file = fopen(new_rec_filename.c_str(), "wb");
		if(new_rec_file) {
			// disable channel/service switch
			combo_channels.set_sensitive(false);	// parent frame already non-sensitive, if channels not available
			btn_channels_stop.set_sensitive(false);	// parent frame already non-sensitive, if channels not available
			combo_services.set_sensitive(false);

			set_icon_name("media-record");
			fprintf(stderr, "DABlinGTK: recording started into '%s'\n", new_rec_filename.c_str());

			{
				std::lock_guard<std::mutex> lock(rec_mutex);

				rec_file = new_rec_file;
				rec_filename = new_rec_filename;

				// write prebuffer
				for(const RecSample& s : rec_prebuffer)
					if(fwrite(&s.data[0], s.data.size(), 1, rec_file) != 1)
						perror("DABlinGTK: error while writing untouched stream prebuffer to file");
				rec_duration_ms = rec_prebuffer_filled_ms;

				// clear prebuffer
				rec_prebuffer.clear();
				rec_prebuffer_filled_ms = 0;

				UpdateRecStatus(true);
			}

			if(options.rec_prebuffer_size_s == 0)
				ensemble_player->AddUntouchedStreamConsumer(this);
		} else {
			int fopen_errno = errno;	// save before overwrite
			tglbtn_record.set_active(false);

			Gtk::MessageDialog hint(*this, "Could not start recording!", false, Gtk::MESSAGE_ERROR);
			hint.set_title("DABlinGTK");
			hint.set_secondary_text(strerror(fopen_errno), false);
			hint.run();
		}
	} else {
		std::lock_guard<std::mutex> lock(rec_mutex);

		// only stop recording, if active (handles error on recording start)
		if(rec_file) {
			if(options.rec_prebuffer_size_s == 0)
				ensemble_player->RemoveUntouchedStreamConsumer(this);

			fclose(rec_file);
			rec_file = nullptr;

			set_icon_name("media-playback-start");
			fprintf(stderr, "DABlinGTK: recording stopped\n");
			UpdateRecStatus(true);

			// enable channel/service switch
			combo_channels.set_sensitive(true);		// parent frame already non-sensitive, if channels not available
			btn_channels_stop.set_sensitive(true);	// parent frame already non-sensitive, if channels not available
			combo_services.set_sensitive(true);
		}
	}
}

void DABlinGTK::ProcessUntouchedStream(const uint8_t* data, size_t len, size_t duration_ms) {
	std::lock_guard<std::mutex> lock(rec_mutex);

	// if recording in progress
	if(rec_file) {
		long int rec_duration_ms_old = rec_duration_ms;

		// write sample
		if(fwrite(data, len, 1, rec_file) != 1)
			perror("DABlinGTK: error while writing untouched stream sample to file");
		rec_duration_ms += duration_ms;

		// update status only on seconds change
		if(rec_duration_ms / 1000 != rec_duration_ms_old / 1000)
			DoRecStatusUpdate(true);
	} else {
		// if prebuffer enabled
		if(options.rec_prebuffer_size_s > 0) {
			long int rec_prebuffer_ms_old = rec_prebuffer_filled_ms;

			// append sample
			rec_prebuffer.emplace_back(data, len, duration_ms);
			rec_prebuffer_filled_ms += duration_ms;

			// remove samples while needed
			while(rec_prebuffer_filled_ms - rec_prebuffer.front().duration_ms >= options.rec_prebuffer_size_s * 1000) {
				rec_prebuffer_filled_ms -= rec_prebuffer.front().duration_ms;
				rec_prebuffer.pop_front();
			}

			// update status only on seconds change
			if(rec_prebuffer_filled_ms / 1000 != rec_prebuffer_ms_old / 1000)
				DoRecStatusUpdate(true);
		}
	}
}

void DABlinGTK::DoRecStatusUpdateEmitted() {
	std::lock_guard<std::mutex> lock(rec_mutex);
	UpdateRecStatus(do_rec_status_update.Pop());
}

void DABlinGTK::UpdateRecStatus(bool decoding) {
	// mutex must already be locked!

	// if recording in progress
	if(rec_file) {
		label_record.set_label(StringTools::MsToTimecode(rec_duration_ms));
		label_record.set_sensitive(true);
		label_record.set_tooltip_text("File name: \"" + rec_filename + "\"");
	} else {
		// if prebuffer enabled and currently decoding
		if(options.rec_prebuffer_size_s > 0 && decoding) {
			label_record.set_label(StringTools::MsToTimecode(rec_prebuffer_filled_ms));
			label_record.set_sensitive(false);
			label_record.set_tooltip_text("Prebuffer: " + StringTools::MsToTimecode(options.rec_prebuffer_size_s * 1000));
		} else {
			label_record.set_label("");
			label_record.set_tooltip_text("");
		}
	}
}


void DABlinGTK::on_tglbtn_mute() {
	ensemble_player->SetAudioMute(tglbtn_mute.get_active());
}

void DABlinGTK::on_vlmbtn(double value) {
	ensemble_player->SetAudioVolume(value);

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

bool DABlinGTK::on_window_delete_event(GdkEventAny* /*any_event*/) {
	// prevent exit while recording
	if(tglbtn_record.get_active()) {
		Gtk::MessageDialog hint(*this, "Cannot exit while recording!", false, Gtk::MESSAGE_ERROR);
		hint.set_title("DABlinGTK");
		hint.set_secondary_text("The recording has to be stopped first.", false);
		hint.run();
		return true;
	} else {
		return false;
	}
}

bool DABlinGTK::HandleKeyPressEvent(GdkEventKey* key_event) {
	// events without Shift/Control/Alt/Win
	if((key_event->state & (Gdk::SHIFT_MASK | Gdk::CONTROL_MASK | Gdk::MOD1_MASK | Gdk::SUPER_MASK)) == 0) {
		switch(key_event->keyval) {
		case GDK_KEY_m:
		case GDK_KEY_M:
			// toggle mute
			tglbtn_mute.clicked();
			return true;
		case GDK_KEY_r:
		case GDK_KEY_R:
			// toggle record, if allowed
			if(tglbtn_record.get_sensitive())
				tglbtn_record.clicked();
			return true;
		}

		// try to switch service (1 to 10, -/+)
		int new_index;
		if(CheckForIndexKey(key_event, combo_services.get_active_row_number(), new_index)) {
			TrySwitch(combo_services, combo_services_liststore, new_index);
			return true;
		}
	}

	// events without Shift/Control/Win, but with Alt
	if((key_event->state & (Gdk::SHIFT_MASK | Gdk::CONTROL_MASK | Gdk::SUPER_MASK)) == 0 && (key_event->state & Gdk::MOD1_MASK)) {
		// try to switch service (11 to 20)
		int new_index;
		if(CheckForIndexKey(key_event, -1, new_index)) {
			TrySwitch(combo_services, combo_services_liststore, new_index + 10);
			return true;
		}
	}

	// events without Shift/Alt/Win, but with Control
	if((key_event->state & (Gdk::SHIFT_MASK | Gdk::MOD1_MASK | Gdk::SUPER_MASK)) == 0 && (key_event->state & Gdk::CONTROL_MASK)) {
		// try to switch channel (1 to 10, -/+)
		int new_index;
		if(CheckForIndexKey(key_event, combo_channels.get_active_row_number(), new_index)) {
			TrySwitch(combo_channels, combo_channels_liststore, new_index);
			return true;
		}

		switch(key_event->keyval) {
		case GDK_KEY_c:
		case GDK_KEY_C: {
			// copy DL to clipboard, if not empty
			std::string dl = label_dl.get_label();
			if(!dl.empty())
				Gtk::Clipboard::get()->set_text(dl);
			return true; }
		case GDK_KEY_space:
			// stop/resume decoding the current channel/service, if allowed
			if(btn_channels_stop.get_sensitive())
				btn_channels_stop.clicked();
			return true;
		}
	}

	// events without Shift/Win, but with Control/Alt
	if((key_event->state & (Gdk::SHIFT_MASK | Gdk::SUPER_MASK)) == 0 && (key_event->state & Gdk::CONTROL_MASK) && (key_event->state & Gdk::MOD1_MASK)) {
		// try to switch channel (11 to 20)
		int new_index;
		if(CheckForIndexKey(key_event, -1, new_index)) {
			TrySwitch(combo_channels, combo_channels_liststore, new_index + 10);
			return true;
		}
	}

	return false;
}

bool DABlinGTK::CheckForIndexKey(GdkEventKey* key_event, int old_index, int& new_index) {
	switch(key_event->keyval) {
	case GDK_KEY_1:
	case GDK_KEY_KP_1:
		new_index = 0;
		return true;
	case GDK_KEY_2:
	case GDK_KEY_KP_2:
		new_index = 1;
		return true;
	case GDK_KEY_3:
	case GDK_KEY_KP_3:
		new_index = 2;
		return true;
	case GDK_KEY_4:
	case GDK_KEY_KP_4:
		new_index = 3;
		return true;
	case GDK_KEY_5:
	case GDK_KEY_KP_5:
		new_index = 4;
		return true;
	case GDK_KEY_6:
	case GDK_KEY_KP_6:
		new_index = 5;
		return true;
	case GDK_KEY_7:
	case GDK_KEY_KP_7:
		new_index = 6;
		return true;
	case GDK_KEY_8:
	case GDK_KEY_KP_8:
		new_index = 7;
		return true;
	case GDK_KEY_9:
	case GDK_KEY_KP_9:
		new_index = 8;
		return true;
	case GDK_KEY_0:
	case GDK_KEY_KP_0:
		new_index = 9;
		return true;
	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		if(old_index == -1)
			break;
		new_index = old_index - 1;
		return true;
	case GDK_KEY_plus:
	case GDK_KEY_KP_Add:
		if(old_index == -1)
			break;
		new_index = old_index + 1;
		return true;
	}
	return false;
}

void DABlinGTK::TrySwitch(Gtk::ComboBox& combo, Glib::RefPtr<Gtk::ListStore>& combo_liststore, int index) {
	// switch, if allowed and index valid
	if(combo.is_sensitive() && index >= 0 && index < (signed) combo_liststore->children().size())
		combo.set_active(index);
}

bool DABlinGTK::HandleConfigureEvent(GdkEventConfigure* /*configure_event*/) {
	// move together with slideshow window
	if(slideshow_window.get_visible())
		slideshow_window.AlignToParent();
	return false;
}

void DABlinGTK::EnsembleUpdateProgressEmitted() {
//	fprintf(stderr, "### EnsembleUpdateProgressEmitted\n");

	ENSEMBLE_PROGRESS progress = ensemble_update_progress.Pop();

	progress_position.set_fraction(progress.value);
	progress_position.set_text(progress.text);
	if(!progress_position.get_visible())
		progress_position.show();
}

void DABlinGTK::EnsembleChangeFormatEmitted() {
//	fprintf(stderr, "### EnsembleChangeFormatEmitted\n");

	label_format.set_label(ensemble_change_format.Pop().GetSummary());
}

void DABlinGTK::FICChangeEnsemble(const FIC_ENSEMBLE& ensemble) {
	fic_change_ensemble.PushAndEmit(ensemble);

	// update LTO for date/time + show
	if(dt_lto != ensemble.lto) {
		dt_lto = ensemble.lto;
//		fprintf(stderr, "### LTO updated\n");
		ShowDateTime(false);
	}
}

void DABlinGTK::FICChangeEnsembleEmitted() {
//	fprintf(stderr, "### FICChangeEnsembleEmitted\n");

	ensemble = fic_change_ensemble.Pop();

	std::string charset_name;
	std::string label = FICDecoder::ConvertLabelToUTF8(ensemble.label, &charset_name);

	std::string tooltip_text =
			"Short label: \"" + FICDecoder::DeriveShortLabelUTF8(label, ensemble.label.short_label_mask) + "\"\n"
			"Label charset: " + charset_name + "\n"
			"EId: " + StringTools::IntToHex(ensemble.eid, 4);
	if(ensemble.ecc != FIC_ENSEMBLE::ecc_none)
		tooltip_text += "\n" "ECC: " + StringTools::IntToHex(ensemble.ecc, 2);
	if(ensemble.lto != FIC_ENSEMBLE::lto_none)
		tooltip_text += "\n" "LTO: " + FICDecoder::ConvertLTOToString(ensemble.lto);
	if(ensemble.inter_table_id != FIC_ENSEMBLE::inter_table_id_none)
		tooltip_text += "\n" "International table ID: " + StringTools::IntToHex(ensemble.inter_table_id, 2) + " (" + FICDecoder::ConvertInterTableIDToString(ensemble.inter_table_id) + ")";

	label_ensemble.set_label(label);
	frame_label_ensemble.set_tooltip_text(tooltip_text);
}

void DABlinGTK::FICChangeServiceEmitted() {
//	fprintf(stderr, "### FICChangeServiceEmitted\n");

	LISTED_SERVICE new_service = fic_change_service.Pop();

	std::string label = FICDecoder::ConvertLabelToUTF8(new_service.label, nullptr);
	std::string combo_label = label;
	if(new_service.multi_comps)
		combo_label = (!new_service.IsPrimary() ? "» " : "") + combo_label + (new_service.IsPrimary() ? " »" : "");

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
	row[combo_services_cols.col_string] = combo_label;
	row[combo_services_cols.col_service] = new_service;

	if(add_new_row) {
		// if first found service requested, adopt service params (for possible later changes)
		if(options.initial_first_found_service) {
			switch_service_sid = new_service.sid;
			switch_service_scids = new_service.scids;
			options.initial_first_found_service = false;
		}

		// set (initial) service
		if(label == switch_service_label || (new_service.sid == switch_service_sid && new_service.scids == switch_service_scids))
			combo_services.set_active(row_it);
	} else {
		// set (updated) service
		Gtk::ListStore::iterator current_it = combo_services.get_active();
		if(current_it && current_it == row_it)
			SetService(new_service);
	}
}

void DABlinGTK::FICChangeUTCDateTime(const FIC_DAB_DT& utc_dt) {
	// (re)sync to DAB date/time + update/show/reschedule
	utc_dt_next = utc_dt;
	dt_update = std::chrono::steady_clock::now();
//	fprintf(stderr, "### time synced\n");
	ShowDateTime(true);
}

void DABlinGTK::ShowDateTime(bool scheduled) {
	// if scheduled, update
	if(scheduled)
		utc_dt_curr = utc_dt_next;

	// if UTC date/time and LTO set, show, if output changed
	if(!utc_dt_curr.IsNone() && dt_lto != FIC_ENSEMBLE::lto_none) {
		std::string dt_str = FICDecoder::ConvertDateTimeToString(utc_dt_curr, dt_lto, false);
//		fprintf(stderr, "### time: %s\n", FICDecoder::ConvertDateTimeToString(utc_dt_curr, dt_lto, true).c_str());
		if(dt_str_prev != dt_str) {
			dt_str_prev = dt_str;
//			fprintf(stderr, "### ----- time displayed: %s\n", dt_str.c_str());
			DoDateTimeUpdate(dt_str);
		}
	}

	// if scheduled, (re)schedule
	if(scheduled) {
		if(!utc_dt_next.IsMsNone()) {
			// long form
			dt_update += std::chrono::milliseconds(1000 - utc_dt_next.ms);
			utc_dt_next.dt.tm_sec++;
			utc_dt_next.ms = 0;
		} else {
			// short form
			dt_update += std::chrono::minutes(1);
			utc_dt_next.dt.tm_min++;
		}
		if(mktime(&utc_dt_next.dt) == (time_t) -1)
			throw std::runtime_error("DABlinGTK: error while normalizing date/time");
	}
}

void DABlinGTK::EnsembleDoRegularWork() {
	// if UTC date/time set and scheduled time reached, update/show/reschedule
	if(!utc_dt_curr.IsNone() && std::chrono::steady_clock::now() >= dt_update)
		ShowDateTime(true);
}

void DABlinGTK::DoDateTimeUpdateEmitted() {
	// display received date/time
	frame_label_datetime.set_sensitive(true);
	label_datetime.set_label(do_datetime_update.Pop());
}

void DABlinGTK::FICDiscardedFIB() {
	fprintf(stderr, "\x1B[33m" "(FIB)" "\x1B[0m" " ");
}

void DABlinGTK::on_combo_channels() {
	btn_channels_stop.set_sensitive(true);

	// cleanup
	if(ensemble_source) {
		ensemble_source->DoExit();
		ensemble_source_thread.join();
		delete ensemble_source;
		ensemble_source = nullptr;
	}

	EnsembleResetFIC();
	combo_services_liststore->clear();	// TODO: prevent on_combo_services() being called for each deleted row
	ensemble_player->StopAudio();
	label_ensemble.set_label("");
	frame_label_ensemble.set_tooltip_text("");

	frame_label_datetime.set_sensitive(false);
	label_datetime.set_label("");

	utc_dt_curr = FIC_DAB_DT();
	dt_lto = FIC_ENSEMBLE::lto_none;
	dt_str_prev = "";

	// apply
	Gtk::TreeModel::iterator row_it = combo_channels.get_active();
	if(combo_channels_liststore->iter_is_valid(row_it)) {
		Gtk::TreeModel::Row row = *row_it;
		DAB_LIVE_SOURCE_CHANNEL channel = row[combo_channels_cols.col_channel];

		combo_channels.set_tooltip_text(
				"Center frequency: " + std::to_string(channel.freq) + " kHz\n"
				"Gain: " + channel.GainToString()
		);

		// prevent re-use of switch service params next time
		if(!switch_service_applied) {
			switch_service_applied = true;
		} else {
			switch_service_label = "";
			switch_service_sid = LISTED_SERVICE::sid_none;
			switch_service_scids = LISTED_SERVICE::scids_none;
		}

		if(options.dab_live_source_type == DABLiveETISource::TYPE_ETI_CMDLINE)
			ensemble_source = new EtiCmdlineETISource(options.dab_live_source_binary, channel, this);
		else
			ensemble_source = new DAB2ETIETISource(options.dab_live_source_binary, channel, this);
		ensemble_source_thread = std::thread(&EnsembleSource::Main, ensemble_source);

		btn_channels_stop.set_image_from_icon_name("media-playback-stop");
	} else {
		combo_channels.set_tooltip_text("");
		btn_channels_stop.set_image_from_icon_name("media-playback-start");
	}
}

void DABlinGTK::on_btn_channels_stop() {
	if(combo_channels.get_active_row_number() != -1) {
		// save state + stop
		Gtk::TreeModel::iterator row_it = combo_services.get_active();
		if(combo_services_liststore->iter_is_valid(row_it)) {
			Gtk::TreeModel::Row row = *row_it;
			LISTED_SERVICE service = row[combo_services_cols.col_service];
			resume_service_sid = service.sid;
			resume_service_scids = service.scids;
		} else {
			resume_service_sid = LISTED_SERVICE::sid_none;
			resume_service_scids = LISTED_SERVICE::scids_none;
		}

		resume_channel_it = combo_channels.get_active();
		combo_channels.unset_active();
	} else {
		// load state + resume
		switch_service_label = "";
		switch_service_sid = resume_service_sid;
		switch_service_scids = resume_service_scids;
		switch_service_applied = false;

		combo_channels.set_active(resume_channel_it);
	}
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

	// consider clear display command
	if(dl.charset != -1) {
		std::string charset_name;
		std::string label = CharsetTools::ConvertTextToUTF8(&dl.raw[0], dl.raw.size(), dl.charset, false, &charset_name);

		// skip unsupported charsets
		if(!charset_name.empty()) {
			frame_label_dl.set_sensitive(true);
			label_dl.set_label(label);
			frame_label_dl.set_tooltip_text("Charset: " + charset_name);
		}
	} else {
		frame_label_dl.set_sensitive(true);
		label_dl.set_label("");
		frame_label_dl.set_tooltip_text("");
	}
}

void DABlinGTK::PADChangeSlideEmitted() {
//	fprintf(stderr, "### PADChangeSlideEmitted\n");

	slideshow_window.UpdateSlide(pad_change_slide.Pop());
	if(tglbtn_slideshow.get_active())
		slideshow_window.TryToShow();
}

void DABlinGTK::PADLengthError(size_t /*announced_xpad_len*/, size_t /*xpad_len*/) {
	fprintf(stderr, "\x1B[31m" "[X-PAD len]" "\x1B[0m" " ");
}
