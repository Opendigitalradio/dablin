/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2019 Stefan Pöschel

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
					"  -d <binary>  Use DAB live source (using the mentioned binary)\n"
					"  -D <type>    DAB live source type: \"%s\" (default), \"%s\"\n"
					"  -C <ch>,...  Channels to be listed (comma separated; requires DAB live source;\n"
					"               an optional gain can also be specified, e.g. \"5C:-54\")\n"
					"  -c <ch>      Channel to be played (requires DAB live source)\n"
					"  -l <label>   Label of the service to be played\n"
					"  -s <sid>     ID of the service to be played\n"
					"  -x <scids>   ID of the service component to be played (requires service ID)\n"
					"  -g <gain>    USB stick gain to pass to DAB live source (auto gain is default)\n"
					"  -r <path>    Path for recordings (default: %s)\n"
					"  -P <size>    Recording prebuffer size in seconds (default: %zu)\n"
					"  -p           Output PCM to stdout instead of using SDL\n"
					"  -u           Output untouched audio stream to stdout instead of using SDL\n"
					"  -I           Don't catch up on stream after interruption\n"
					"  -S           Initially disable slideshow\n"
					"  -L           Enable loose behaviour (e.g. PAD conformance)\n"
					"  file         Input file to be played (stdin, if not specified)\n",
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

	// option args
	int c;
	while((c = getopt(argc, argv, "hd:D:C:c:l:g:r:P:s:x:puISL")) != -1) {
		switch(c) {
		case 'h':
			usage(argv[0]);
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
		case 's':
			options.initial_sid = strtol(optarg, nullptr, 0);
			initial_param_count++;
			break;
		case 'x':
			options.initial_scids = strtol(optarg, nullptr, 0);
			break;
		case 'g':
			options.gain = strtol(optarg, nullptr, 0);
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


	// at most one initial param needed!
	if(initial_param_count > 1) {
		fprintf(stderr, "At most one initial parameter shall be specified!\n");
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

	initial_channel_appended = false;
	rec_file = nullptr;
	rec_duration_ms = 0;
	rec_prebuffer_filled_ms = 0;

	slideshow_window.set_transient_for(*this);

	eti_update_progress.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::ETIUpdateProgressEmitted));
	eti_change_format.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::ETIChangeFormatEmitted));
	fic_change_ensemble.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeEnsembleEmitted));
	fic_change_service.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::FICChangeServiceEmitted));
	pad_change_dynamic_label.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::PADChangeDynamicLabelEmitted));
	pad_change_slide.GetDispatcher().connect(sigc::mem_fun(*this, &DABlinGTK::PADChangeSlideEmitted));

	eti_player = new ETIPlayer(options.pcm_output, options.untouched_output, options.disable_int_catch_up, this);

	if(!options.dab_live_source_binary.empty()) {
		eti_source = nullptr;
	} else {
		eti_source = new ETISource(options.filename, this);
		eti_source_thread = std::thread(&ETISource::Main, eti_source);
	}

	fic_decoder = new FICDecoder(this);
	pad_decoder = new PADDecoder(this, options.loose);

	set_title("DABlin v" + std::string(DABLIN_VERSION));
	set_icon_name("media-playback-stop");

	InitWidgets();

	set_border_width(2 * WIDGET_SPACE);

	// combobox first must be visible
	if(combo_channels_liststore->iter_is_valid(initial_channel_it))
		combo_channels.set_active(initial_channel_it);
	initial_channel_appended = true;

	// add window key press event handler
	signal_key_press_event().connect(sigc::mem_fun(*this, &DABlinGTK::HandleKeyPressEvent));
	add_events(Gdk::KEY_PRESS_MASK);

	// add window config event handler (before default handler)
	signal_configure_event().connect(sigc::mem_fun(*this, &DABlinGTK::HandleConfigureEvent), false);
	add_events(Gdk::STRUCTURE_MASK);
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
	frame_combo_channels.set_size_request(75, -1);
	frame_combo_channels.add(combo_channels);

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
	top_grid.attach_next_to(tglbtn_record, frame_label_format, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(tglbtn_mute, tglbtn_record, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(vlmbtn, tglbtn_mute, Gtk::POS_RIGHT, 1, 1);
	top_grid.attach_next_to(tglbtn_slideshow, tglbtn_record, Gtk::POS_BOTTOM, 3, 1);
	top_grid.attach_next_to(frame_label_dl, frame_combo_channels, Gtk::POS_BOTTOM, 7, 1);
	top_grid.attach_next_to(progress_position, frame_label_dl, Gtk::POS_BOTTOM, 7, 1);

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
		set_icon_name("media-playback-start");

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

	// if the audio service changed, reset format/DL/slide + switch
	if(!eti_player->IsSameAudioService(service.audio_service)) {
		// stop playback of old service
		if(options.rec_prebuffer_size_s > 0)
			eti_player->RemoveUntouchedStreamConsumer(this);
		eti_player->SetAudioService(AUDIO_SERVICE());

		pad_decoder->Reset();

		label_format.set_label("");

		frame_label_dl.set_sensitive(false);
		label_dl.set_label("");
		frame_label_dl.set_tooltip_text("");

		if(!service.HasSLS()) {
			slideshow_window.hide();
			slideshow_window.ClearSlide();
		}

		if(options.rec_prebuffer_size_s > 0) {
			std::lock_guard<std::mutex> lock(rec_mutex);

			// clear prebuffer
			rec_prebuffer.clear();
			rec_prebuffer_filled_ms = 0;
		}

		// start playback of new service
		eti_player->SetAudioService(service.audio_service);
		if(options.rec_prebuffer_size_s > 0)
			eti_player->AddUntouchedStreamConsumer(this);
	}

	if(service.HasSLS()) {
		slideshow_window.AwaitSlide();
		if(tglbtn_slideshow.get_active())
			slideshow_window.TryToShow();
		pad_decoder->SetMOTAppType(service.sls_app_type);
	}
}


void DABlinGTK::on_tglbtn_record() {
	if(tglbtn_record.get_active()) {
		// get timestamp
		const time_t now = time(nullptr);
		if(now == (time_t) -1)
			perror("DABlinGTK: error while getting time");
		struct tm* now_tm = localtime(&now);
		if(!now_tm)
			perror("DABlinGTK: error while getting local time");
		char now_string[22];
		strftime(now_string, sizeof(now_string), "%F - %H-%M-%S", now_tm);

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

		std::string new_rec_filename = options.recordings_path + "/" + std::string(now_string) + " - " + label_cleaned + "." + eti_player->GetUntouchedStreamFileExtension();
		FILE* new_rec_file = fopen(new_rec_filename.c_str(), "wb");
		if(new_rec_file) {
			// disable channel/service switch
			combo_channels.set_sensitive(false);	// parent frame already non-sensitive, if channels not available
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

				UpdateRecStatus();
			}

			if(options.rec_prebuffer_size_s == 0)
				eti_player->AddUntouchedStreamConsumer(this);
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
				eti_player->RemoveUntouchedStreamConsumer(this);

			fclose(rec_file);
			rec_file = nullptr;

			set_icon_name("media-playback-start");
			fprintf(stderr, "DABlinGTK: recording stopped\n");
			UpdateRecStatus();

			// enable channel/service switch
			combo_channels.set_sensitive(true);		// parent frame already non-sensitive, if channels not available
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
			UpdateRecStatus();
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
				UpdateRecStatus();
		}
	}
}

void DABlinGTK::UpdateRecStatus() {
	// mutex must already be locked!

	std::string tooltip_text;

	// if recording in progress
	if(rec_file) {
		tooltip_text =
				"File name: \"" + rec_filename + "\"\n"
				"Duration: " + StringTools::MsToTimecode(rec_duration_ms);
	} else {
		// if prebuffer enabled
		if(rec_prebuffer_filled_ms > 0)
			tooltip_text = "Prebuffer: " + StringTools::MsToTimecode(rec_prebuffer_filled_ms) + " / " + StringTools::MsToTimecode(options.rec_prebuffer_size_s * 1000);
	}

	tglbtn_record.set_tooltip_text(tooltip_text);
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
		case GDK_KEY_C:
			// copy DL to clipboard, if not empty
			std::string dl = label_dl.get_label();
			if(!dl.empty())
				Gtk::Clipboard::get()->set_text(dl);
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

	label_format.set_label(eti_change_format.Pop().GetSummary());
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
		// set (initial) service
		if(label == options.initial_label || (new_service.sid == options.initial_sid && new_service.scids == options.initial_scids))
			combo_services.set_active(row_it);
	} else {
		// set (updated) service
		Gtk::ListStore::iterator current_it = combo_services.get_active();
		if(current_it && current_it == row_it)
			SetService(new_service);
	}
}

void DABlinGTK::FICDiscardedFIB() {
	fprintf(stderr, "\x1B[33m" "(FIB)" "\x1B[0m" " ");
}

void DABlinGTK::on_combo_channels() {
	Gtk::TreeModel::Row row = *combo_channels.get_active();
	DAB_LIVE_SOURCE_CHANNEL channel = row[combo_channels_cols.col_channel];

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
	frame_combo_channels.set_tooltip_text(
			"Center frequency: " + std::to_string(channel.freq) + " kHz\n"
			"Gain: " + channel.GainToString()
	);

	// prevent re-use of initial params
	if(initial_channel_appended) {
		options.initial_label = "";
		options.initial_sid = LISTED_SERVICE::sid_none;
		options.initial_scids = LISTED_SERVICE::scids_none;
	}

	// append
	if(options.dab_live_source_type == DABLiveETISource::TYPE_ETI_CMDLINE)
		eti_source = new EtiCmdlineETISource(options.dab_live_source_binary, channel, this);
	else
		eti_source = new DAB2ETIETISource(options.dab_live_source_binary, channel, this);
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
