/*
    DABlin - capital DAB experience
    Copyright (C) 2018-2019 Stefan Pöschel

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

#ifndef DABLIN_GTK_SLS_H_
#define DABLIN_GTK_SLS_H_

#include <atomic>
#include <string>

#include <gtkmm.h>

#include "mot_manager.h"



// --- DABlinGTKSlideshowWindow -----------------------------------------------------------------
class DABlinGTKSlideshowWindow : public Gtk::Window {
private:
	Gtk::Grid top_grid;
	Gtk::Image image;
	Gtk::LinkButton link_button;

	Glib::RefPtr<Gdk::Pixbuf> pixbuf_waiting;
	std::atomic<int> offset_x;
	std::atomic<int> offset_y;

	bool HandleKeyPressEvent(GdkEventKey* key_event);
	bool HandleConfigureEvent(GdkEventConfigure* configure_event);
public:
	DABlinGTKSlideshowWindow();

	void TryToShow();
	void AlignToParent();

	void AwaitSlide();
	void UpdateSlide(const MOT_FILE& slide);
	void ClearSlide() {image.clear();}
	bool IsEmptySlide() {return image.get_storage_type() == Gtk::ImageType::IMAGE_EMPTY;}
};


#endif /* DABLIN_GTK_SLS_H_ */
