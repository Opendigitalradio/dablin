/*
    DABlin - capital DAB experience
    Copyright (C) 2021 Stefan Pöschel

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

#ifndef DABLIN_GTK_DL_PLUS_H_
#define DABLIN_GTK_DL_PLUS_H_

#include <string>

#include <gtkmm.h>

#include "pad_decoder.h"



// --- DABlinGTKDLPlusObjectColumns -----------------------------------------------------------------
class DABlinGTKDLPlusObjectColumns : public Gtk::TreeModelColumnRecord {
public:
	Gtk::TreeModelColumn<int> col_content_type;
	Gtk::TreeModelColumn<std::string> col_text;
	Gtk::TreeModelColumn<bool> col_deleted;
	Gtk::TreeModelColumn<bool> col_item_running;

	DABlinGTKDLPlusObjectColumns() {
		add(col_content_type);
		add(col_text);
		add(col_deleted);
		add(col_item_running);
	}
};


// --- DABlinGTKDLPlusWindow -----------------------------------------------------------------
class DABlinGTKDLPlusWindow : public Gtk::Window {
private:
	Gtk::Grid top_grid;

	DABlinGTKDLPlusObjectColumns list_dl_plus_cols;
	Glib::RefPtr<Gtk::ListStore> list_dl_plus_liststore;
	Gtk::TreeView list_dl_plus;
	Gtk::CellRendererText cell_renderer_text;
	void RenderContentType(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);
	void RenderText(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);

	int prev_parent_x;
	int prev_parent_y;

	bool HandleKeyPressEvent(GdkEventKey* key_event);
public:
	DABlinGTKDLPlusWindow();

	void TryToShow();
	void AlignToParent();

	void UpdateDLPlusInfo(const DL_STATE& dl);
	void ClearDLPlusInfo();
};


#endif /* DABLIN_GTK_DL_PLUS_H_ */
