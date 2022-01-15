/*
    DABlin - capital DAB experience
    Copyright (C) 2021-2022 Stefan Pöschel

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

#include "dablin_gtk_dl_plus.h"



// --- DABlinGTKDLPlusWindow -----------------------------------------------------------------
DABlinGTKDLPlusWindow::DABlinGTKDLPlusWindow() {
	prev_parent_x = -1;
	prev_parent_y = -1;

	set_title("Dynamic Label Plus");
	set_type_hint(Gdk::WINDOW_TYPE_HINT_UTILITY);
	set_resizable(false);
	set_deletable(false);

	list_dl_plus_liststore = Gtk::ListStore::create(list_dl_plus_cols);
	list_dl_plus_liststore->set_sort_column(list_dl_plus_cols.col_content_type, Gtk::SORT_ASCENDING);

	list_dl_plus.set_model(list_dl_plus_liststore);
	list_dl_plus.set_size_request(250, -1);
	list_dl_plus.insert_column_with_data_func(-1, "Content type", cell_renderer_text, sigc::mem_fun(*this, &DABlinGTKDLPlusWindow::RenderContentType));
	list_dl_plus.insert_column_with_data_func(-1, "Text", cell_renderer_text, sigc::mem_fun(*this, &DABlinGTKDLPlusWindow::RenderText));

	add(top_grid);

	top_grid.attach(list_dl_plus, 0, 0, 1, 1);

	show_all_children();

	// add window key press event handler
	signal_key_press_event().connect(sigc::mem_fun(*this, &DABlinGTKDLPlusWindow::HandleKeyPressEvent));
	add_events(Gdk::KEY_PRESS_MASK);
}

void DABlinGTKDLPlusWindow::RenderContentType(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter) {
	Gtk::CellRendererText *rt = (Gtk::CellRendererText*) cell;

	int content_type = (*iter)[list_dl_plus_cols.col_content_type];
	bool item_running = (*iter)[list_dl_plus_cols.col_item_running];

	rt->property_text() = DynamicLabelDecoder::ConvertDLPlusContentTypeToString(content_type);
	rt->property_family() = "Monospace";
	rt->property_foreground_rgba() = Gdk::RGBA("black");
	rt->property_background_rgba() = (content_type < 12 && item_running) ? Gdk::RGBA("light green") : Gdk::RGBA("grey90");	// highlight running ITEM objects
	rt->property_weight() = Pango::WEIGHT_NORMAL;
}

void DABlinGTKDLPlusWindow::RenderText(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter) {
	Gtk::CellRendererText *rt = (Gtk::CellRendererText*) cell;

	int content_type = (*iter)[list_dl_plus_cols.col_content_type];
	std::string text = (*iter)[list_dl_plus_cols.col_text];
	bool deleted = (*iter)[list_dl_plus_cols.col_deleted];

	// color highlighting
	std::string color = "black";
	if(content_type == 1)	// ITEM.TITLE
		color = "blue";
	if(content_type >= 12 && content_type < 31)	// INFO
		color = "green";
	if(content_type >= 41 && content_type < 54)	// INTERACTIVITY
		color = "blue";

	rt->property_text() = text;
	rt->property_family() = "";
	rt->property_foreground_rgba() = Gdk::RGBA(color);
	rt->property_background_rgba() = Gdk::RGBA(deleted ? "light grey" : "white");	// indicate deleted objects
	rt->property_weight() = content_type < 12 ? Pango::WEIGHT_BOLD : Pango::WEIGHT_NORMAL;	// highlight ITEM objects
}

void DABlinGTKDLPlusWindow::TryToShow() {
	// if already visible or no DL Plus info present, abort
	if(get_visible() || get_tooltip_text().empty())
		return;

	AlignToParent();
	show();
}

void DABlinGTKDLPlusWindow::AlignToParent() {
	int x, y, w, h, this_x, this_y;
	get_transient_for()->get_position(x, y);
	get_transient_for()->get_size(w, h);
	get_position(this_x, this_y);	// event position doesn't work!

	int curr_parent_x = x;
	int curr_parent_y = y + h;

	// TODO: fix multi head issue
	if(prev_parent_x == -1 && prev_parent_y == -1) {
		// first: align to the bottom of parent
		move(curr_parent_x, curr_parent_y + 40);	// add some vertical padding for WM decoration
	} else {
		// later: follow parent
		move(curr_parent_x + (this_x - prev_parent_x), curr_parent_y + (this_y - prev_parent_y));
	}

	prev_parent_x = curr_parent_x;
	prev_parent_y = curr_parent_y;
}

bool DABlinGTKDLPlusWindow::HandleKeyPressEvent(GdkEventKey* key_event) {
	// events without Shift/Alt/Win, but with Control
	if((key_event->state & (Gdk::SHIFT_MASK | Gdk::MOD1_MASK | Gdk::SUPER_MASK)) == 0 && (key_event->state & Gdk::CONTROL_MASK)) {
		switch(key_event->keyval) {
		case GDK_KEY_c:
		case GDK_KEY_C:
			// copy DL Plus object text, if entry selected
			const Gtk::TreeModel::iterator& row_it = list_dl_plus.get_selection()->get_selected();
			if(list_dl_plus_liststore->iter_is_valid(row_it)) {
				std::string text = (*row_it)[list_dl_plus_cols.col_text];
				Gtk::Clipboard::get()->set_text(text);
			}
			return true;
		}
	}
	return false;
}

void DABlinGTKDLPlusWindow::UpdateDLPlusInfo(const DL_STATE& dl) {
	// add/update DL Plus objects
	Gtk::ListStore::Children children = list_dl_plus_liststore->children();

	// process objects
	for(const DL_PLUS_OBJECT& obj : dl.dl_plus_objects) {
		// ignore DUMMY tags
		if(obj.content_type == 0)
			continue;

		bool deleted = obj.text == " ";

		// get row (add new one, if needed and no deletion)
		Gtk::ListStore::iterator row_it = std::find_if(
				children.begin(), children.end(),
				[&](const Gtk::TreeRow& row)->bool {
					return row[list_dl_plus_cols.col_content_type] == obj.content_type;
				}
		);
		bool add_new_row = row_it == children.end();
		if(add_new_row && !deleted)
			row_it = list_dl_plus_liststore->append();

		// continue, if no existing object for received deletion
		if(row_it == children.end())
			continue;

		Gtk::TreeRow row = *row_it;
		if(add_new_row)
			row[list_dl_plus_cols.col_content_type] = obj.content_type;
		if(!deleted)
			row[list_dl_plus_cols.col_text] = obj.text;
		row[list_dl_plus_cols.col_deleted] = deleted;
	}

	// forward item running flag to all existing objects
	for(const Gtk::TreeRow& row : children)
		row[list_dl_plus_cols.col_item_running] = dl.dl_plus_ir;

	set_tooltip_text(
			"Item toggle bit: " + std::to_string(dl.dl_plus_it ? 1 : 0) + "\n"
			"Item running bit: " + std::to_string(dl.dl_plus_ir ? 1 : 0)
	);
}

void DABlinGTKDLPlusWindow::ClearDLPlusInfo() {
	list_dl_plus_liststore->clear();
	list_dl_plus.columns_autosize();
	list_dl_plus.check_resize();	// prevent resizing bug
	set_tooltip_text("");
}
