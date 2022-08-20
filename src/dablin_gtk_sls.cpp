/*
    DABlin - capital DAB experience
    Copyright (C) 2018-2022 Stefan Pöschel

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

#include "dablin_gtk_sls.h"



// --- DABlinGTKSlideshowWindow -----------------------------------------------------------------
DABlinGTKSlideshowWindow::DABlinGTKSlideshowWindow() {
	pixbuf_waiting = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 320, 240);	// default slide size
	pixbuf_waiting->fill(0x003000FF);

	prev_parent_x = -1;
	prev_parent_y = -1;

	set_type_hint(Gdk::WINDOW_TYPE_HINT_UTILITY);
	set_resizable(false);
	set_deletable(false);

	add(top_grid);

	top_grid.attach(image, 0, 0, 1, 1);
	top_grid.attach_next_to(link_button, image, Gtk::POS_BOTTOM, 1, 1);
	top_grid.attach_next_to(progress_file, link_button, Gtk::POS_BOTTOM, 1, 1);

	show_all_children();

	// add window key press event handler
	signal_key_press_event().connect(sigc::mem_fun(*this, &DABlinGTKSlideshowWindow::HandleKeyPressEvent));
	add_events(Gdk::KEY_PRESS_MASK);
}

void DABlinGTKSlideshowWindow::TryToShow() {
	// if already visible or no slide (awaiting), abort
	if(get_visible() || IsEmptySlide())
		return;

	AlignToParent();
	show();
}

void DABlinGTKSlideshowWindow::AlignToParent() {
	int x, y, w, h, this_x, this_y;
	get_transient_for()->get_position(x, y);
	get_transient_for()->get_size(w, h);
	get_position(this_x, this_y);	// event position doesn't work!

	int curr_parent_x = x + w;
	int curr_parent_y = y;

	// TODO: fix multi head issue
	if(prev_parent_x == -1 && prev_parent_y == -1) {
		// first: align to the right of parent
		move(curr_parent_x + 20, curr_parent_y);	// add some horizontal padding for WM decoration
	} else {
		// later: follow parent
		move(curr_parent_x + (this_x - prev_parent_x), curr_parent_y + (this_y - prev_parent_y));
	}

	prev_parent_x = curr_parent_x;
	prev_parent_y = curr_parent_y;
}

bool DABlinGTKSlideshowWindow::HandleKeyPressEvent(GdkEventKey* key_event) {
	// events without Shift/Alt/Win, but with Control
	if((key_event->state & (Gdk::SHIFT_MASK | Gdk::MOD1_MASK | Gdk::SUPER_MASK)) == 0 && (key_event->state & Gdk::CONTROL_MASK)) {
		switch(key_event->keyval) {
		case GDK_KEY_c:
		case GDK_KEY_C:
			// copy slide to clipboard, if not empty
			Glib::RefPtr<Gdk::Pixbuf> pb = image.get_pixbuf();
			if(pb != pixbuf_waiting)
				Gtk::Clipboard::get()->set_image(pb);
			return true;
		}
	}
	return false;
}

void DABlinGTKSlideshowWindow::AwaitSlide() {
	set_title("Slideshow...");

	image.set(pixbuf_waiting);
	image.set_tooltip_text("Waiting for slide...");

	link_button.hide();
	progress_file.hide();
}

void DABlinGTKSlideshowWindow::UpdateSlide(const MOT_FILE& slide) {
	std::string type_mime = "";
	std::string type_display = "unknown";

	switch(slide.content_sub_type) {
	case MOT_FILE::CONTENT_SUB_TYPE_JFIF:
		type_mime = "image/jpeg";
		type_display = "JPEG";
		break;
	case MOT_FILE::CONTENT_SUB_TYPE_PNG:
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
			"Resolution: " + std::to_string(pixbuf->get_width()) + "x" + std::to_string(pixbuf->get_height()) + " pixels\n"
			"Size: " + std::to_string(slide.data.size()) + " bytes\n"
			"Format: " + type_display + "\n"
			"Content name: \"" + slide.content_name + "\"\n"
			"Content name charset: " + slide.content_name_charset);

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

	// hide file progress
	progress_file.hide();
}

void DABlinGTKSlideshowWindow::UpdateFileProgress(const double fraction) {
	// update/show file progress
	if(fraction == -1)
		progress_file.pulse();	// unknown progress
	else
		progress_file.set_fraction(fraction);
	progress_file.show();
}
