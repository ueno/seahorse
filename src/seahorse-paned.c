/*
 * Seahorse
 *
 * Copyright (C) 2015 Daiki Ueno
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-paned.h"

#include "libseahorse/seahorse-key-manager-store.h"
#include "seahorse-sidebar.h"

enum {
	PROP_0,
	PROP_BACKENDS
};

struct _SeahorsePaned
{
	GtkPaned parent;

	SeahorseSidebar *sidebar;
	GtkAlignment *area;
	GtkTreeView *view;
	GcrUnionCollection *collection;
	SeahorseKeyManagerStore *store;
	SeahorsePredicate pred;

	GList *backends;
	GSettings *settings;
};

struct _SeahorsePanedClass
{
	GtkPanedClass parent;
};

G_DEFINE_TYPE (SeahorsePaned, seahorse_paned, GTK_TYPE_PANED);

static void
seahorse_paned_init (SeahorsePaned *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
	self->settings = g_settings_new ("org.gnome.seahorse.manager");
}

static void
on_view_row_activated (GtkTreeView *view, GtkTreePath *path,
		       GtkTreeViewColumn *column, SeahorsePaned *self)
{
	GObject* obj;

	g_return_if_fail (SEAHORSE_IS_PANED (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));

	obj = seahorse_key_manager_store_get_object_from_path (self->view, path);
	if (obj != NULL) {
		GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		seahorse_catalog_show_properties (SEAHORSE_CATALOG (toplevel), obj);
	}
}

static gboolean
on_view_button_pressed (GtkTreeView *view, GdkEventButton *event,
			SeahorsePaned *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PANED (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);

	if (event->button == 3) {
		GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		seahorse_catalog_show_object_menu (SEAHORSE_CATALOG (toplevel),
						   event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static gboolean
on_view_popup_menu (GtkTreeView *view, SeahorsePaned *self)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	GList *objects;

	objects = seahorse_paned_get_selected_objects (self);
	if (objects != NULL)
		seahorse_catalog_show_object_menu (SEAHORSE_CATALOG (toplevel),
		                                   0, gtk_get_current_event_time ());
	g_list_free (objects);
	return FALSE;
}

static void
on_sidebar_popup_menu (SeahorseSidebar *sidebar,
                       GcrCollection *collection,
                       gpointer user_data)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (sidebar));
	seahorse_catalog_show_collection_menu (SEAHORSE_CATALOG (toplevel),
					       collection,
					       0,
					       gtk_get_current_event_time ());
}

static void
seahorse_paned_constructed (GObject *object)
{
	SeahorsePaned *self = SEAHORSE_PANED (object);
	GtkTreeSelection *selection;

	G_OBJECT_CLASS (seahorse_paned_parent_class)->constructed (object);

	self->collection = GCR_UNION_COLLECTION (gcr_union_collection_new ());
	self->sidebar = seahorse_sidebar_new (self->backends, self->collection);
	g_signal_connect (self->sidebar, "context-menu",
			  G_CALLBACK (on_sidebar_popup_menu), self);
	gtk_container_add (GTK_CONTAINER (self->area),
			   GTK_WIDGET (self->sidebar));
	gtk_widget_show (GTK_WIDGET (self->sidebar));
	gtk_widget_show (GTK_WIDGET (self->area));

	selection = gtk_tree_view_get_selection (self->view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (self->view, "button-press-event",
	                  G_CALLBACK (on_view_button_pressed), self);
	g_signal_connect (self->view, "row-activated",
	                  G_CALLBACK (on_view_row_activated), self);
	g_signal_connect (self->view, "popup-menu",
	                  G_CALLBACK (on_view_popup_menu), self);

	gtk_widget_show (GTK_WIDGET (self->view));

	/* Add new key store and associate it */
	self->store = seahorse_key_manager_store_new (GCR_COLLECTION (self->collection),
						      self->view,
						      &self->pred,
						      self->settings);

	g_settings_bind (self->settings, "sidebar-visible",
			 self->area, "visible",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "sidebar-visible",
			 self->sidebar, "combined",
			 G_SETTINGS_BIND_INVERT_BOOLEAN);

	g_settings_bind (self->settings, "keyrings-selected",
			 self->sidebar, "selected-uris",
			 G_SETTINGS_BIND_DEFAULT);
}

static void
seahorse_paned_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	SeahorsePaned *self = SEAHORSE_PANED (object);
	switch (prop_id) {
	case PROP_BACKENDS:
		self->backends = g_list_copy (g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_paned_dispose (GObject *object)
{
	SeahorsePaned *self = SEAHORSE_PANED (object);
	g_clear_object (&self->collection);
	g_clear_object (&self->store);
	g_clear_pointer (&self->backends, g_list_free);
	g_clear_object (&self->settings);
	G_OBJECT_CLASS (seahorse_paned_parent_class)->dispose (object);
}

static void
seahorse_paned_class_init (SeahorsePanedClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = seahorse_paned_constructed;
	object_class->set_property = seahorse_paned_set_property;
	object_class->dispose = seahorse_paned_dispose;

	g_object_class_install_property (object_class, PROP_BACKENDS,
	        g_param_spec_pointer ("backends", "Backends", "Backends",
				      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-paned.xml");
	gtk_widget_class_bind_template_child_full (widget_class,
						   "sidebar-area",
						   FALSE,
						   G_STRUCT_OFFSET (SeahorsePaned, area));
	gtk_widget_class_bind_template_child_full (widget_class,
						   "key-list-tree-view",
						   FALSE,
						   G_STRUCT_OFFSET (SeahorsePaned, view));
}

GList *
seahorse_paned_get_selected_objects (SeahorsePaned *self)
{
	return seahorse_key_manager_store_get_selected_objects (self->view);
}

void
seahorse_paned_set_filter_flags (SeahorsePaned *self, SeahorseFlags flags)
{
	self->pred.flags = flags;
	seahorse_key_manager_store_refilter (self->store);
}

void
seahorse_paned_set_filter_text (SeahorsePaned *self, const gchar *text)
{
	g_object_set (self->store, "filter", text, NULL);
}

SeahorsePlace *
seahorse_paned_get_focused_place (SeahorsePaned *self)
{
	return seahorse_sidebar_get_focused_place (self->sidebar);
}

SeahorsePaned *
seahorse_paned_new (GList *backends)
{
	return g_object_new (SEAHORSE_TYPE_PANED,
			     "backends", backends,
	                     NULL);
}
