/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse {

namespace Util {

	public void show_error (Gtk.Widget? parent,
	                        string? heading,
	                        string? message) {
		Gtk.Window? window = null;

		if (message == null)
			message = "";

		if (parent != null) {
			if (!(parent is Gtk.Window))
				parent = parent.get_toplevel();
			if (parent is Gtk.Window)
				window = (Gtk.Window)parent;
		}

		var dialog = new Gtk.MessageDialog(window, Gtk.DialogFlags.MODAL,
		                                   Gtk.MessageType.ERROR,
		                                   Gtk.ButtonsType.CLOSE, "");
		if (heading == null)
			dialog.set("text", message);
		else
			dialog.set("text", heading, "secondary-text", message);

		dialog.run();
		dialog.destroy();
	}

	public string get_display_date_string (long time)
	{
		if (time == 0)
			return "";
		var created_date = GLib.Date();
		created_date.set_time_t (time);
		var buffer = new char[128];
		created_date.strftime(buffer, _("%Y-%m-%d"));
		return (string)buffer;
	}

	public Gtk.Builder load_built_contents(Gtk.Container? frame,
	                                       string name) {
		var builder = new Gtk.Builder();
		string path = "/org/gnome/Seahorse/seahorse-%s.xml".printf(name);

		if (frame != null && frame is Gtk.Dialog)
			frame = ((Gtk.Dialog)frame).get_content_area();

		try {
			builder.add_from_resource(path);
			var obj = builder.get_object(name);
			if (obj == null) {
				GLib.critical("Couldn't find object named %s in %s", name, path);
			} else if (frame != null) {
				var widget = (Gtk.Widget)obj;
				frame.add(widget);
				widget.show();
			}
		} catch (GLib.Error err) {
			GLib.critical("Couldn't load %s: %s", path, err.message);
		}

		return builder;
	}

	private GLib.MenuModel? find_submenu_model(GLib.MenuModel model,
											   string submodel_id)
	{
		GLib.MenuModel? insertion_model = null;
		for (var i = 0; i < model.get_n_items(); i++) {
			var value = model.get_item_attribute_value(i, "id", GLib.VariantType.STRING);
			if (value != null && value.get_string() == submodel_id) {
				insertion_model = model.get_item_link(i, GLib.Menu.LINK_SECTION);
				if (insertion_model == null)
					insertion_model = model.get_item_link(i, GLib.Menu.LINK_SUBMENU);
				if (insertion_model != null)
					break;
			} else {
				var submodel = model.get_item_link(i, GLib.Menu.LINK_SECTION);
				if (submodel == null)
					submodel = model.get_item_link(i, GLib.Menu.LINK_SUBMENU);
				if (submodel == null)
					continue;
				for (var j = 0; j < submodel.get_n_items(); j++) {
					var submenu = model.get_item_link(i, GLib.Menu.LINK_SUBMENU);
					if (submenu != null)
						insertion_model = find_submenu_model(submenu, submodel_id);
					if (insertion_model != null)
						break;
				}
			}
		}
		return insertion_model;
	}

	public void merge_menu(GLib.Menu original,
						   GLib.Menu gmenu_to_merge,
						   string submodel_name,
						   bool prepend)
	{
		var submodel = Util.find_submenu_model(original, submodel_name);
		if (submodel == null)
			return;
		for (var i = 0; i < gmenu_to_merge.get_n_items(); i++) {
			var item = new GLib.MenuItem.from_model(gmenu_to_merge, i);
			if (prepend)
				((GLib.Menu) submodel).prepend_item(item);
			else
				((GLib.Menu) submodel).append_item(item);
		}
	}
}

}
