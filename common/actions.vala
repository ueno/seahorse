/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse {

public class Action {
	public static void pre_activate(GLib.Action action,
	                                Catalog? catalog,
	                                Gtk.Window? window) {
		action.set_data("seahorse-action-window", window);
		action.set_data("seahorse-action-catalog", catalog);
	}

	public static void activate_with_window(GLib.Action action,
	                                        Catalog? catalog,
	                                        Gtk.Window? window) {
		pre_activate(action, catalog, window);
		action.activate(null);
		post_activate(action);
	}

	public static void post_activate(GLib.Action action) {
		action.set_data("seahorse-action-window", null);
		action.set_data("seahorse-action-catalog", null);
	}

	public static Gtk.Window? get_window(GLib.Action action) {
		Gtk.Window? window = action.get_data("seahorse-action-window");
		return window;
	}

	public static Catalog? get_catalog(GLib.Action action) {
		Catalog? catalog = action.get_data("seahorse-action-catalog");
		return catalog;
	}
}

}
