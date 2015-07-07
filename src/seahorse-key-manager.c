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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "libseahorse/seahorse-application.h"
#include "libseahorse/seahorse-collection.h"
#include "libseahorse/seahorse-key-manager-store.h"
#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include "seahorse-generate-select.h"
#include "seahorse-import-dialog.h"
#include "seahorse-key-manager.h"
#include "seahorse-sidebar.h"

#include <glib/gi18n.h>

enum {
	SHOW_ANY,
	SHOW_PERSONAL,
	SHOW_TRUSTED,
};

struct _SeahorseKeyManagerPrivate {
	GSimpleAction *show_action;
	GtkEntry* filter_entry;
	SeahorsePredicate pred;
	SeahorseSidebar *sidebar;

	GtkTreeView* view;
	GcrCollection *collection; /* owned by the sidebar */
	SeahorseKeyManagerStore* store;

	GSettings *settings;
	gint sidebar_width;
	guint sidebar_width_sig;
};

enum  {
	TARGETS_PLAIN,
	TARGETS_URIS
};

G_DEFINE_TYPE (SeahorseKeyManager, seahorse_key_manager, SEAHORSE_TYPE_CATALOG);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gboolean 
fire_selection_changed (gpointer user_data)
{
	SeahorseKeyManager* self = SEAHORSE_KEY_MANAGER (user_data);

	/* Fire the signal */
	g_signal_emit_by_name (self, "selection-changed");
	
	/* This is called as a one-time idle handler, return FALSE so we don't get run again */
	return FALSE;
}

static void 
on_view_selection_changed (GtkTreeSelection* selection, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_idle_add ((GSourceFunc)fire_selection_changed, self);
}

static void
on_keymanager_row_activated (GtkTreeView* view, GtkTreePath* path, 
                                  GtkTreeViewColumn* column, SeahorseKeyManager* self) 
{
	GObject* obj;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));
	
	obj = seahorse_key_manager_store_get_object_from_path (view, path);
	if (obj != NULL)
		seahorse_catalog_show_properties (SEAHORSE_CATALOG (self), obj);
}

static gboolean
on_keymanager_key_list_button_pressed (GtkTreeView* view, GdkEventButton* event, SeahorseKeyManager* self) 
{
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);
	
	if (event->button == 3)
		seahorse_catalog_show_object_menu (SEAHORSE_CATALOG (self),
						   event->button, event->time);

	return FALSE;
}

static gboolean
on_keymanager_key_list_popup_menu (GtkTreeView* view, SeahorseKeyManager* self) 
{
	GList *objects;

	objects = seahorse_catalog_get_selected_objects (SEAHORSE_CATALOG (self));
	if (objects != NULL)
		seahorse_catalog_show_object_menu (SEAHORSE_CATALOG (self),
		                                   0, gtk_get_current_event_time ());
	g_list_free (objects);
	return FALSE;
}

static void 
on_file_new (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (G_IS_SIMPLE_ACTION (action));
	seahorse_generate_select_show (seahorse_catalog_get_window (SEAHORSE_CATALOG (self)));
}

static void
on_keymanager_new_button (GtkButton* button, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_BUTTON (button));
	seahorse_generate_select_show (seahorse_catalog_get_window (SEAHORSE_CATALOG (self)));
}

#if REFACTOR_FIRST
static gboolean 
on_first_timer (SeahorseKeyManager* self) 
{
	GtkWidget* widget;

	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	
	/* 
	 * Although not all the keys have completed we'll know whether we have 
	 * any or not at this point 
	 */

	if (gcr_collection_get_length (GCR_COLLECTION (self->pv->collection)) == 0) {
		widget = xxwidget_get_widget (XX_WIDGET (self), "first-time-box");
		gtk_widget_show (widget);
	}
	
	return FALSE;
}
#endif

static void
on_clear_clicked (GtkEntry* entry,
                  GtkEntryIconPosition icon_pos,
                  GdkEvent* event,
                  gpointer user_data)
{
	gtk_entry_set_text (entry, "");
}

static void 
on_filter_changed (GtkEntry* entry,
                   SeahorseKeyManager* self)
{
	const gchar *text;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ENTRY (entry));

	text = gtk_entry_get_text (entry);
	g_object_set (self->pv->store, "filter", text, NULL);

	if (text == NULL || g_str_equal (text, "")) {
		g_object_set (G_OBJECT (entry),
		              "secondary-icon-name", "edit-find-symbolic",
		              "secondary-icon-activatable", FALSE,
		              "secondary-icon-sensitive", FALSE,
		              NULL);
	} else {
		g_object_set (G_OBJECT (entry),
		              "secondary-icon-name", "edit-clear-symbolic",
		              "secondary-icon-activatable", TRUE,
		              "secondary-icon-sensitive", TRUE,
		              NULL);
	}
}

static gboolean
on_start_interactive_search (GtkTreeView *treeview,
                             gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	gtk_widget_grab_focus (GTK_WIDGET (self->pv->filter_entry));
	return FALSE;
}

static void 
import_files (SeahorseKeyManager* self,
              const gchar** uris)
{
	GtkDialog *dialog;
	GtkWindow *parent;

	parent = seahorse_catalog_get_window (SEAHORSE_CATALOG (self));
	dialog = seahorse_import_dialog_new (parent);
	seahorse_import_dialog_add_uris (SEAHORSE_IMPORT_DIALOG (dialog), uris);

	gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void 
import_prompt (SeahorseKeyManager* self) 
{
	GtkFileFilter *filter;
	GtkWidget *dialog;
	gchar *uris[2];
	gchar *uri = NULL;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));

	dialog = gtk_file_chooser_dialog_new (_("Import Key"),
	                                      seahorse_catalog_get_window (SEAHORSE_CATALOG (self)),
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                      NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);

	/* TODO: This should come from libgcr somehow */
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All key files"));
	gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
	gtk_file_filter_add_mime_type (filter, "application/x-ssh-key");
	gtk_file_filter_add_mime_type (filter, "application/pkcs12");
	gtk_file_filter_add_mime_type (filter, "application/pkcs12+pem");
	gtk_file_filter_add_mime_type (filter, "application/pkcs7-mime");
	gtk_file_filter_add_mime_type (filter, "application/pkcs7-mime+pem");
	gtk_file_filter_add_mime_type (filter, "application/pkcs8");
	gtk_file_filter_add_mime_type (filter, "application/pkcs8+pem");
	gtk_file_filter_add_mime_type (filter, "application/pkix-cert");
	gtk_file_filter_add_mime_type (filter, "application/pkix-cert+pem");
	gtk_file_filter_add_mime_type (filter, "application/pkix-crl");
	gtk_file_filter_add_mime_type (filter, "application/pkix-crl+pem");
	gtk_file_filter_add_mime_type (filter, "application/x-pem-file");
	gtk_file_filter_add_mime_type (filter, "application/x-pem-key");
	gtk_file_filter_add_mime_type (filter, "application/x-pkcs12");
	gtk_file_filter_add_mime_type (filter, "application/x-pkcs7-certificates");
	gtk_file_filter_add_mime_type (filter, "application/x-x509-ca-cert");
	gtk_file_filter_add_mime_type (filter, "application/x-x509-user-cert");
	gtk_file_filter_add_mime_type (filter, "application/pkcs10");
	gtk_file_filter_add_mime_type (filter, "application/pkcs10+pem");
	gtk_file_filter_add_mime_type (filter, "application/x-spkac");
	gtk_file_filter_add_mime_type (filter, "application/x-spkac+base64");
	gtk_file_filter_add_pattern (filter, "*.asc");
	gtk_file_filter_add_pattern (filter, "*.gpg");
	gtk_file_filter_add_pattern (filter, "*.pub");
	gtk_file_filter_add_pattern (filter, "*.pfx");
	gtk_file_filter_add_pattern (filter, "*.cer");
	gtk_file_filter_add_pattern (filter, "*.crt");
	gtk_file_filter_add_pattern (filter, "*.pem");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

	gtk_widget_destroy (dialog);

	if (uri != NULL) {
		uris[0] = uri;
		uris[1] = NULL;
		import_files (self, (const gchar**)uris);
	}

	g_free (uri);
}

static void 
on_key_import_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (G_IS_SIMPLE_ACTION (action));
	import_prompt (self);
}

static void
on_keymanager_import_button (GtkButton* button, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_BUTTON (button));
	import_prompt (self);
}

static void 
import_text (SeahorseKeyManager* self,
             const gchar *display_name,
             const char* text)
{
	GtkDialog *dialog;
	GtkWindow *parent;

	parent = seahorse_catalog_get_window (SEAHORSE_CATALOG (self));
	dialog = seahorse_import_dialog_new (parent);
	seahorse_import_dialog_add_text (SEAHORSE_IMPORT_DIALOG (dialog),
	                                 display_name, text);

	gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void 
on_target_drag_data_received (GtkWindow* window, GdkDragContext* context, gint x, gint y, 
                              GtkSelectionData* selection_data, guint info, guint time_,
                              SeahorseKeyManager* self) 
{
	guchar *text;
	gchar **uris;
	gchar **uri;
	
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (selection_data != NULL);
	
	if (info == TARGETS_PLAIN) {
		text = gtk_selection_data_get_text (selection_data);
		import_text (self, _("Dropped text"), (gchar*)text);
		g_free (text);
	} else if (info == TARGETS_URIS) {
		uris = gtk_selection_data_get_uris (selection_data);
		for (uri = uris; *uri; ++uri) 
			g_strstrip (*uri);
		import_files (self, (const gchar**)uris);
		g_strfreev (uris);
	}
}

static void
update_clipboard_state (GtkClipboard* clipboard, GdkEvent* event, GActionGroup *group)
{
	GAction *action;
	gboolean text_available;

	text_available = gtk_clipboard_wait_is_text_available (clipboard);
	action = g_action_map_lookup_action (G_ACTION_MAP (group),
					     "edit-import-clipboard");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), text_available);
}

static void 
on_clipboard_received (GtkClipboard* board, const char* text, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_CLIPBOARD (board));

	if (text == NULL)
		return;

	g_assert(self->pv->filter_entry);
	if (gtk_widget_is_focus (GTK_WIDGET (self->pv->filter_entry)) == TRUE)
		gtk_editable_paste_clipboard (GTK_EDITABLE (self->pv->filter_entry));
	else
		if (text != NULL && g_utf8_strlen (text, -1) > 0)
			import_text (self, _("Clipboard text"), text);
}

static void 
on_key_import_clipboard (GSimpleAction *action, GVariant *parameter,
			 gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	GdkAtom atom;
	GtkClipboard* board;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (G_IS_SIMPLE_ACTION (action));

	atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	board = gtk_clipboard_get (atom);
	gtk_clipboard_request_text (board, (GtkClipboardTextReceivedFunc)on_clipboard_received, self);
}

static void 
on_app_quit (GSimpleAction *action, GVariant *parameter,
             gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (G_IS_SIMPLE_ACTION (action));
	g_application_quit (G_APPLICATION (seahorse_application_get ()));
}

static void
on_view_sidebar_changed (GSimpleAction *action,
			 GVariant *state,
			 gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	g_settings_set_boolean (self->pv->settings, "sidebar-visible",
				g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
update_view_filter (SeahorseKeyManager *self)
{
	GVariant *state;
	const gchar *value;

	state = g_action_get_state (G_ACTION (self->pv->show_action));
	value = g_variant_get_string (state, NULL);
	if (strcmp (value, "personal") == 0)
		self->pv->pred.flags = SEAHORSE_FLAG_PERSONAL;
	else if (strcmp (value, "trusted") == 0)
		self->pv->pred.flags = SEAHORSE_FLAG_TRUSTED;
	else
		self->pv->pred.flags = 0;

	seahorse_key_manager_store_refilter (self->pv->store);
}


static void
on_view_show_changed (GSimpleAction *action,
		      GVariant *state,
                      gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	g_settings_set_string (self->pv->settings, "item-filter",
			       g_variant_get_string (state, NULL));
	g_simple_action_set_state (action, state);
	update_view_filter (self);
}

static void
on_item_filter_changed (GSettings *settings,
                        const gchar *key,
                        gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	gchar *value;

	value = g_settings_get_string (settings, key);
	if (!(g_str_equal (value, "any") ||
	      g_str_equal (value, "personal") ||
	      g_str_equal (value, "trusted"))) {
		g_free (value);
		value = g_strdup ("any");
	}

	g_simple_action_set_state (self->pv->show_action,
				   g_variant_new_take_string (value));
	update_view_filter (self);
}

static const GActionEntry GENERAL_ACTIONS[] = {
	{ "quit", on_app_quit, NULL, NULL, NULL },
	{ "file-new", on_file_new, NULL, NULL, NULL },
	{ "new-object", on_file_new, NULL, NULL, NULL },
	{ "file-import", on_key_import_file, NULL, NULL, NULL },
	{ "edit-import-clipboard", on_key_import_clipboard, NULL, NULL, NULL },
	{ "view-sidebar", NULL, NULL, "false", on_view_sidebar_changed },
	{ "view-show", NULL, "s", "'personal'", on_view_show_changed }
};

static GList *
seahorse_key_manager_get_selected_objects (SeahorseCatalog *catalog)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (catalog);
	return seahorse_key_manager_store_get_selected_objects (self->pv->view);
}

static SeahorsePlace *
seahorse_key_manager_get_focused_place (SeahorseCatalog *catalog)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (catalog);
	return seahorse_sidebar_get_focused_place (self->pv->sidebar);
}

static gboolean
on_panes_realize (GtkWidget *widget,
                  gpointer   user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	gint width;

	width = g_settings_get_int (self->pv->settings, "sidebar-width");
	gtk_paned_set_position (GTK_PANED (widget), width);

	return FALSE;
}

static gboolean
on_panes_unrealize (GtkWidget *widget,
                    gpointer   user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	gint width;

	width = gtk_paned_get_position (GTK_PANED (widget));
	g_settings_set_int (self->pv->settings, "sidebar-width", width);

	return FALSE;
}

static void
on_sidebar_popup_menu (SeahorseSidebar *sidebar,
                       GcrCollection *collection,
                       gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);

	seahorse_catalog_show_collection_menu (SEAHORSE_CATALOG (self),
					       collection,
					       0,
					       gtk_get_current_event_time ());
}

static GcrCollection *
setup_sidebar (SeahorseKeyManager *self)
{
	GtkWidget *area, *panes;
	GActionGroup *actions;
	GList *backends, *l;
	GtkBuilder *builder;

	self->pv->sidebar = seahorse_sidebar_new ();

	self->pv->sidebar_width = g_settings_get_int (self->pv->settings, "sidebar-width");
	builder = seahorse_catalog_get_builder (SEAHORSE_CATALOG (self));
	panes = GTK_WIDGET (gtk_builder_get_object (builder, "sidebar-panes"));
	gtk_paned_set_position (GTK_PANED (panes), self->pv->sidebar_width);
	g_signal_connect (panes, "realize", G_CALLBACK (on_panes_realize), self);
	g_signal_connect (panes, "unrealize", G_CALLBACK (on_panes_unrealize), self);
	g_signal_connect (self->pv->sidebar, "context-menu", G_CALLBACK (on_sidebar_popup_menu), self);

	gtk_widget_set_size_request (gtk_paned_get_child1 (GTK_PANED (panes)), 50, -1);
	gtk_widget_set_size_request (gtk_paned_get_child2 (GTK_PANED (panes)), 150, -1);

	backends = seahorse_sidebar_get_backends (self->pv->sidebar);
	for (l = backends; l != NULL; l = g_list_next (l)) {
		gchar *name;
		actions = NULL;
		g_object_get (l->data,
			      "name", &name,
			      "actions", &actions,
			      NULL);
		if (actions != NULL) {
			g_object_set_data (G_OBJECT (actions),
					   "seahorse-action-catalog",
					   self);
			gtk_widget_insert_action_group (GTK_WIDGET (self),
							name,
							actions);
			g_object_unref (actions);
		}
		g_signal_emit_by_name (l->data,
				       "included",
				       SEAHORSE_CATALOG (self));
		g_free (name);
	}

	area = GTK_WIDGET (gtk_builder_get_object (builder, "sidebar-area"));
	gtk_container_add (GTK_CONTAINER (area), GTK_WIDGET (self->pv->sidebar));
	gtk_widget_show (GTK_WIDGET (self->pv->sidebar));
	g_settings_bind (self->pv->settings, "sidebar-visible",
	                 area, "visible",
	                 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->pv->settings, "sidebar-visible",
	                 self->pv->sidebar, "combined",
	                 G_SETTINGS_BIND_INVERT_BOOLEAN);

	g_settings_bind (self->pv->settings, "keyrings-selected",
	                 self->pv->sidebar, "selected-uris",
	                 G_SETTINGS_BIND_DEFAULT);

	return seahorse_sidebar_get_collection (self->pv->sidebar);
}

static void
seahorse_key_manager_constructed (GObject *object)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (object);
	GActionGroup* actions;
	GtkTargetList* targets;
	GtkTreeSelection *selection;
	GAction *action;
	GtkWindow *window;
	GtkBuilder *builder;
	GtkClipboard* clipboard;
	GtkWidget *widget;

	G_OBJECT_CLASS (seahorse_key_manager_parent_class)->constructed (object);

	window = seahorse_catalog_get_window (SEAHORSE_CATALOG (self));
	gtk_window_set_default_geometry(window, 640, 476);
	gtk_widget_set_events (GTK_WIDGET (window), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_window_set_title (seahorse_catalog_get_window (SEAHORSE_CATALOG (self)), _("Passwords and Keys"));

	self->pv->collection = setup_sidebar (self);

	/* Init key list & selection settings */
	builder = seahorse_catalog_get_builder (SEAHORSE_CATALOG (self));
	self->pv->view = GTK_TREE_VIEW (gtk_builder_get_object (builder, "key-list"));
	g_return_if_fail (self->pv->view != NULL);

	selection = gtk_tree_view_get_selection (self->pv->view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection, "changed", G_CALLBACK (on_view_selection_changed), self);
	gtk_widget_realize (GTK_WIDGET (self->pv->view));

	/* Add new key store and associate it */
	self->pv->store = seahorse_key_manager_store_new (self->pv->collection,
	                                                  self->pv->view,
	                                                  &self->pv->pred,
	                                                  self->pv->settings);

	g_object_get (self, "actions", &actions, NULL);
	g_action_map_add_action_entries (G_ACTION_MAP (actions),
					 GENERAL_ACTIONS,
					 G_N_ELEMENTS (GENERAL_ACTIONS),
					 self);
	gtk_widget_insert_action_group (GTK_WIDGET (self), "app", actions);
	g_object_unref (actions);

	/* Notify us when settings change */
	action = g_action_map_lookup_action (G_ACTION_MAP (actions), "view-sidebar");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (g_settings_get_boolean (self->pv->settings, "sidebar-visible")));

	action = g_action_map_lookup_action (G_ACTION_MAP (actions), "view-show");
	self->pv->show_action = G_SIMPLE_ACTION (action);
	g_signal_connect_object (self->pv->settings, "changed::item-filter",
	                         G_CALLBACK (on_item_filter_changed), self, 0);
	on_item_filter_changed (self->pv->settings, "item-filter", self);

	/* first time signals */
	g_signal_connect_object (gtk_builder_get_object (builder, "import-button"),
	                         "clicked", G_CALLBACK (on_keymanager_import_button), self, 0);

	g_signal_connect_object (gtk_builder_get_object (builder, "new-button"),
	                         "clicked", G_CALLBACK (on_keymanager_new_button), self, 0);

	/* Make sure import is only available with clipboard content */
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	g_signal_connect (clipboard, "owner-change", G_CALLBACK (update_clipboard_state), actions);
	update_clipboard_state (clipboard, NULL, actions);

	/* Find the menubar */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "menu-placeholder"));
	if (widget) {
		GtkWidget *menubar = gtk_menu_bar_new_from_model (G_MENU_MODEL (gtk_builder_get_object (builder, "menubar")));
		gtk_container_add (GTK_CONTAINER (widget), menubar);
		gtk_widget_show_all (menubar);
	}

	/* Find the toolbar */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "new-object-button"));
	gtk_actionable_set_action_name (GTK_ACTIONABLE (widget), "app.new-object");
	gtk_widget_set_tooltip_text (widget, _("Add a new key or item"));
	self->pv->filter_entry = GTK_ENTRY (gtk_builder_get_object (builder, "filter-entry"));
	on_filter_changed (self->pv->filter_entry, self);
	gtk_entry_set_width_chars (self->pv->filter_entry, 30);
	g_signal_connect (self->pv->filter_entry, "icon-release",
	                  G_CALLBACK (on_clear_clicked), NULL);

	/* For the filtering */
	g_signal_connect_object (GTK_EDITABLE (self->pv->filter_entry), "changed", 
	                         G_CALLBACK (on_filter_changed), self, 0);
	g_signal_connect (self->pv->view, "start-interactive-search",
	                  G_CALLBACK (on_start_interactive_search), self);

	/* Set focus to the current key list */
	gtk_widget_grab_focus (GTK_WIDGET (self->pv->view));
	g_signal_emit_by_name (self, "selection-changed");

	/* To avoid flicker */
	gtk_widget_show (GTK_WIDGET (self));
	
	/* Setup drops */
	gtk_drag_dest_set (GTK_WIDGET (seahorse_catalog_get_window (SEAHORSE_CATALOG (self))),
	                   GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	targets = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (targets, TARGETS_URIS);
	gtk_target_list_add_text_targets (targets, TARGETS_PLAIN);
	gtk_drag_dest_set_target_list (GTK_WIDGET (seahorse_catalog_get_window (SEAHORSE_CATALOG (self))), targets);

	g_signal_connect_object (seahorse_catalog_get_window (SEAHORSE_CATALOG (self)), "drag-data-received",
	                         G_CALLBACK (on_target_drag_data_received), self, 0);

	g_signal_connect (self->pv->view, "button-press-event",
	                  G_CALLBACK (on_keymanager_key_list_button_pressed), self);
	g_signal_connect (self->pv->view, "row-activated",
	                  G_CALLBACK (on_keymanager_row_activated), self);
	g_signal_connect (self->pv->view, "popup-menu",
	                  G_CALLBACK (on_keymanager_key_list_popup_menu), self);

#ifdef REFACTOR_FIRST
	/* To show first time dialog */
	g_timeout_add_seconds (1, (GSourceFunc)on_first_timer, self);
#endif
}

static void
seahorse_key_manager_init (SeahorseKeyManager *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerPrivate);
	self->pv->settings = g_settings_new ("org.gnome.seahorse.manager");
}

static void
seahorse_key_manager_finalize (GObject *obj)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (obj);

	if (self->pv->sidebar_width_sig != 0) {
		g_source_remove (self->pv->sidebar_width_sig);
		self->pv->sidebar_width_sig = 0;
	}

	self->pv->filter_entry = NULL;

	g_clear_object (&self->pv->settings);

	G_OBJECT_CLASS (seahorse_key_manager_parent_class)->finalize (obj);
}

static void
seahorse_key_manager_class_init (SeahorseKeyManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseCatalogClass *catalog_class = SEAHORSE_CATALOG_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorseKeyManagerPrivate));

	gobject_class->constructed = seahorse_key_manager_constructed;
	gobject_class->finalize = seahorse_key_manager_finalize;

	catalog_class->get_selected_objects = seahorse_key_manager_get_selected_objects;
	catalog_class->get_focused_place = seahorse_key_manager_get_focused_place;
}

SeahorseKeyManager *
seahorse_key_manager_show (guint32 timestamp)
{
	static SeahorseKeyManager *singleton;

	if (singleton) {
		gtk_window_present_with_time (GTK_WINDOW (singleton), timestamp);
		return singleton;
	} else {
		singleton = g_object_new (SEAHORSE_TYPE_KEY_MANAGER, "ui-name", "key-manager", NULL);
		g_object_add_weak_pointer (G_OBJECT (singleton), (gpointer*) &singleton);
		return singleton;
	}
}
