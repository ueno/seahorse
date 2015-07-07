/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2013 Red Hat Inc.
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@redhat.com>
 */

namespace Seahorse {
namespace Pkcs11 {

public class Properties : Gtk.Window {
	public Gck.Object object { construct; get; }

	private Gtk.Box _content;
	private Gcr.Viewer _viewer;
	private GLib.Cancellable _cancellable;
	private Gck.Object _request_key;
	private GLib.ActionGroup _actions;

	public Properties(Gck.Object object,
	                  Gtk.Window window) {
		GLib.Object(object: object, transient_for: window);
	}

	construct {
		this._cancellable = new GLib.Cancellable();
		set_default_size (400, 400);

		this._content = new Gtk.Box(Gtk.Orientation.VERTICAL, 0);
		this.add(this._content);
		this._content.show();

		this._viewer = Gcr.Viewer.new_scrolled();
		this._content.add(this._viewer);
		this._viewer.set_hexpand(true);
		this._viewer.set_vexpand(true);
		this._viewer.show();

		var toolbar = new Gtk.Toolbar();

		var gicon = new GLib.ThemedIcon(Gtk.Stock.SAVE_AS);
		var icon = new Gtk.Image.from_gicon(gicon, Gtk.IconSize.SMALL_TOOLBAR);
		var item = new Gtk.ToolButton(icon, _("_Export"));
		item.set_action_name("%s.export-object".printf(PKCS11_NAME));
		item.set_tooltip_text(_("Export the certificate"));
		toolbar.add(item);

		gicon = new GLib.ThemedIcon(Gtk.Stock.DELETE);
		icon = new Gtk.Image.from_gicon(gicon, Gtk.IconSize.SMALL_TOOLBAR);
		item = new Gtk.ToolButton(icon, _("_Delete"));
		item.set_action_name("%s.delete-object".printf(PKCS11_NAME));
		item.set_tooltip_text(_("Delete this certificate or key"));
		toolbar.add(item);

		toolbar.add(new Gtk.SeparatorToolItem());

		item = new Gtk.ToolButton(null, _("Request _Certificate"));
		item.set_action_name("%s.request-certificate".printf(PKCS11_NAME));
		item.set_tooltip_text(_("Create a certificate request file for this key"));

		this._content.pack_start(toolbar, false, true, 0);
		this._content.reorder_child(toolbar, 0);

		toolbar.get_style_context().add_class(Gtk.STYLE_CLASS_PRIMARY_TOOLBAR);
		toolbar.reset_style();
		toolbar.show_all();

		/* ... */

		this._actions = new GLib.SimpleActionGroup();
		((GLib.ActionMap) this._actions).add_action_entries(UI_ACTIONS, this);
		var action = ((GLib.ActionMap) this._actions).lookup_action("delete-object");
		this.object.bind_property("deletable", action, "enabled",
		                          GLib.BindingFlags.SYNC_CREATE);
		action = ((GLib.ActionMap) this._actions).lookup_action("export-object");
		this.object.bind_property("exportable", action, "enabled",
		                          GLib.BindingFlags.SYNC_CREATE);
		var request = ((GLib.ActionMap) this._actions).lookup_action("request-certificate");
		((GLib.SimpleAction) request).set_enabled(false);
		this.insert_action_group(PKCS11_NAME, this._actions);

		this.object.notify["label"].connect(() => { this.update_label(); });
		this.update_label();
		this.add_renderer_for_object(this.object);
		this.check_certificate_request_capable(this.object);

		GLib.Object? partner;
		this.object.get("partner", out partner);
		if (partner != null) {
			this.add_renderer_for_object(partner);
			this.check_certificate_request_capable(partner);
		}

		GLib.List<Exporter> exporters;
		if (this.object is Exportable)
			exporters = ((Exportable)this.object).create_exporters(ExporterType.ANY);

		var export = ((GLib.ActionMap) this._actions).lookup_action("export-object");
		((GLib.SimpleAction) export).set_enabled(exporters != null);

		this._viewer.grab_focus();
	}

	public override void dispose() {
		this._cancellable.cancel();
		base.dispose();
	}

	private void update_label() {
		string? label;
		string? description;
		this.object.get("label", out label, "description", out description);
		if (label == null || label == "")
			label = _("Unnamed");
		this.set_title("%s - %s".printf(label, description));
	}

	private void add_renderer_for_object(GLib.Object object) {
		Gck.Attributes? attributes = null;
		string? label = null;

		object.get("label", &label, "attributes", &attributes);
		if (attributes != null) {
			var renderer = Gcr.Renderer.create(label, attributes);
			if (renderer != null) {
				object.bind_property("label", renderer, "label",
				                     GLib.BindingFlags.DEFAULT);
				object.bind_property("attributes", renderer, "attributes",
				                     GLib.BindingFlags.DEFAULT);

				if (renderer.get_class().find_property("object") != null)
					renderer.set("object", object);

				this._viewer.add_renderer(renderer);
			}
		}
	}

	private void on_export_certificate(GLib.SimpleAction action, GLib.Variant? parameter) {
		GLib.List<GLib.Object> objects = null;
		objects.append(this.object);
		try {
			Exportable.export_to_prompt_wait(objects, this);
		} catch (GLib.Error err) {
			Util.show_error(this, _("Failed to export certificate"), err.message);
		}
	}

	private void on_delete_objects(GLib.SimpleAction action, GLib.Variant? parameter) {
		GLib.Object? partner;
		this.object.get("partner", out partner);

		Deleter deleter;
		if (partner != null || this.object is PrivateKey) {
			deleter = new KeyDeleter((Gck.Object)this.object);
			if (!deleter.add_object(partner))
				GLib.assert_not_reached();
		} else {
			deleter = new Deleter((Gck.Object)this.object);
		}

		if (deleter.prompt(this)) {
			deleter.delete.begin(this._cancellable, (obj, res) => {
				try {
					if (deleter.delete.end(res))
						this.destroy();
				} catch (GLib.Error err) {
					Util.show_error(this, _("Couldn't delete"), err.message);
				}
			});
		}
	}

	private void on_request_certificate(GLib.SimpleAction action, GLib.Variant? parameter) {
		Request.prompt(this, this._request_key);
	}

	private static const GLib.ActionEntry[] UI_ACTIONS = {
		{ "export-object", on_export_certificate, null, null, null },
		{ "delete-object", on_delete_objects, null, null, null },
		{ "request-certificate", on_request_certificate, null, null, null }
	};

	private void check_certificate_request_capable(GLib.Object object) {
		if (!(object is PrivateKey))
			return;

		Gcr.CertificateRequest.capable_async.begin((PrivateKey)object, this._cancellable, (obj, res) => {
			try {
				if (Gcr.CertificateRequest.capable_async.end(res)) {
					var request = ((GLib.ActionMap) this._actions).lookup_action("request-certificate");
					((GLib.SimpleAction) request).set_enabled(true);
					this._request_key = (PrivateKey)object;
				}
			} catch (GLib.Error err) {
				GLib.message("couldn't check capabilities of private key: %s", err.message);
			}
		});
	}
}

}
}
