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

public abstract class Catalog : Gtk.Window {
	private static const string MENU_POPUP = "popup";
	private static const string MENU_MENUBAR = "menubar";
	private static const string MENU_POPOVER = "popover";

	/* For compatibility with old code */
	public Gtk.Window window {
		get { return this; }
	}

	/* Set by the derived classes */
	public string ui_name { construct; get; }

	public GLib.ActionGroup actions {
		owned get {
			return this._actions;
		}
	}

	private Gtk.Builder _builder;
	private GLib.HashTable<string,GLib.Menu> _menus;
	private GLib.ActionGroup _actions;
	private GLib.SimpleAction _edit_delete;
	private GLib.SimpleAction _properties_object;
	private GLib.SimpleAction _file_export;
	private GLib.SimpleAction _edit_copy;
	private bool _disposed;
	private GLib.Settings _settings;

	public abstract GLib.List<weak Backend> get_backends();
	public abstract Place? get_focused_place();
	public abstract GLib.List<weak GLib.Object> get_selected_objects();

	construct {
		this._builder = Util.load_built_contents(this, this.ui_name);
		this._menus = new GLib.HashTable<string, GLib.Menu>(GLib.str_hash, GLib.str_equal);

		var menubar = this._builder.get_object("menubar");
		this._menus.set(MENU_MENUBAR, (GLib.Menu) menubar);

		var popup = this._builder.get_object("popup");
		this._menus.set(MENU_POPUP, (GLib.Menu) popup);

		var popover = this._builder.get_object("popover");
		this._menus.set(MENU_POPOVER, (GLib.Menu) popover);

		/* Load window size for windows that aren't dialogs */
		var key = "/apps/seahorse/windows/%s/".printf(this.ui_name);
		this._settings = new GLib.Settings.with_path("org.gnome.seahorse.window", key);
		var width = this._settings.get_int("width");
		var height = this._settings.get_int("height");
		if (width > 0 && height > 0)
			this.resize (width, height);

		this._actions = new GLib.SimpleActionGroup();
		var map = (GLib.ActionMap) this._actions;
		map.add_action_entries(COMMON_ENTRIES, this);

		this._edit_delete = (GLib.SimpleAction) map.lookup_action("edit-delete");
		this._properties_object = (GLib.SimpleAction) map.lookup_action("properties-object");
		this._edit_copy = (GLib.SimpleAction) map.lookup_action("edit-export-clipboard");
		this._file_export = (GLib.SimpleAction) map.lookup_action("file-export");

		Seahorse.Application.get().add_window(this);
	}

	public override void dispose() {
		this._edit_copy = null;
		this._edit_delete = null;
		this._file_export = null;
		this._properties_object = null;

		if (!this._disposed) {
			this._disposed = true;

			int width, height;
			this.get_size(out width, out height);
			this._settings.set_int("width", width);
			this._settings.set_int("height", height);
			Seahorse.Application.get().remove_window (this);
		}

		base.dispose();
	}

	public virtual signal void selection_changed() {
		bool can_properties = false;
		bool can_delete = false;
		bool can_export = false;

		var objects = this.get_selected_objects();
		foreach (var object in objects) {
			if (Exportable.can_export(object))
				can_export = true;
			if (Deletable.can_delete(object))
				can_delete = true;
			if (Viewable.can_view(object))
				can_properties = true;
			if (can_export && can_delete && can_properties)
				break;
		}
		this._properties_object.set_enabled(can_properties);
		this._edit_delete.set_enabled(can_delete);
		this._edit_copy.set_enabled(can_export);
		this._file_export.set_enabled(can_export);
	}

	public unowned Gtk.Builder get_builder() {
		return this._builder;
	}

	public void update_menu(string menu_name,
							string submodel_name,
							GLib.MenuModel model_to_merge)
	{
		var model = this._menus.get(menu_name);
		if (model == null)
			return;
		Util.merge_menu((GLib.Menu) model,
						(GLib.Menu) model_to_merge,
						submodel_name,
						false);
	}

	public void register_collection_menu(Gcr.Collection collection,
										 GLib.MenuModel model)
	{
		this._menus.set (collection.get_type().name(), (GLib.Menu) model);
	}

	public void show_properties(GLib.Object obj) {
		Viewable.view(obj, this);
	}

	private void show_context_menu(string name, uint button, uint time) {
		var model = this._menus.get(name);

		if (model == null)
			return;
		var menu = new Gtk.Menu.from_model(model);
		menu.attach_to_widget(this, null);
		((Gtk.Menu)menu).popup(null, null, null, button, time);
	}

	public void show_collection_menu(Gcr.Collection collection,
									 uint button,
									 uint time)
	{
		show_context_menu(collection.get_type().name(), button, time);
	}

	public void show_object_menu(uint button, uint time) {
		show_context_menu(MENU_POPUP, button, time);
	}

	[CCode (instance_pos = -1)]
	private void on_object_delete(GLib.SimpleAction action,
								  GLib.Variant? parameter)
	{
		try {
			var objects = this.get_selected_objects();
			Deletable.delete_with_prompt_wait(objects, this);
		} catch (GLib.Error err) {
			Util.show_error(window, _("Cannot delete"), err.message);
		}
	}

	[CCode (instance_pos = -1)]
	private void on_properties_object(GLib.SimpleAction action,
									  GLib.Variant? parameter)
	{
		var objects = get_selected_objects();
		if (objects.length() > 0)
			this.show_properties(objects.data);
	}

	[CCode (instance_pos = -1)]
	private void on_properties_place (GLib.SimpleAction action,
									  GLib.Variant? parameter)
	{
		var place = this.get_focused_place ();
		if (place != null)
			this.show_properties (place);
	}

	[CCode (instance_pos = -1)]
	private void on_key_export_file (GLib.SimpleAction action,
									 GLib.Variant? parameter)
	{
		try {
			Exportable.export_to_prompt_wait(this.get_selected_objects(), this);
		} catch (GLib.Error err) {
			Util.show_error(window, _("Couldn't export keys"), err.message);
		}
	}

	[CCode (instance_pos = -1)]
	private void on_key_export_clipboard (GLib.SimpleAction action,
										  GLib.Variant? parameter)
	{
		uint8[] output;
		try {
			var objects = this.get_selected_objects ();
			Exportable.export_to_text_wait (objects, out output);
		} catch (GLib.Error err) {
			Util.show_error(this, _("Couldn't export data"), err.message);
			return;
		}

		/* TODO: Print message if only partially exported */

		var board = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD);
		board.set_text ((string)output, output.length);
	}

	private static const GLib.ActionEntry[] COMMON_ENTRIES = {
		{ "file-export", on_key_export_file, null, null, null },
		{ "edit-export-clipboard", on_key_export_clipboard, null, null, null },
		{ "edit-delete", on_object_delete, null, null, null },
		{ "properties-object", on_properties_object, null, null, null },
		{ "properties-keyring", on_properties_place, null, null, null }
	};
}

}
