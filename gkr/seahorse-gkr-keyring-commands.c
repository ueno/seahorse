/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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
 */

#include "config.h"

#include "seahorse-gkr-keyring-commands.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-dialogs.h"

#include "common/seahorse-registry.h"

#include "seahorse-source.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

enum {
	PROP_0
};

G_DEFINE_TYPE (SeahorseGkrKeyringCommands, seahorse_gkr_keyring_commands, SEAHORSE_TYPE_COMMANDS);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
on_gkr_add_keyring (GtkAction *action, gpointer unused)
{
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_gkr_add_keyring_show (NULL);
}

static const GtkActionEntry ACTION_ENTRIES[] = {
	{ "gkr-add-keyring", "folder", N_ ("Password Keyring"), "", 
	  N_("Used to store application and network passwords"), G_CALLBACK (on_gkr_add_keyring) }
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gkr_keyring_commands_show_properties (SeahorseCommands* base, SeahorseObject* object) 
{
	GtkWindow *window;

	g_return_if_fail (SEAHORSE_IS_OBJECT (object));
	g_return_if_fail (seahorse_object_get_tag (object) == SEAHORSE_GKR_TYPE);

	window = seahorse_view_get_window (seahorse_commands_get_view (base));
	if (G_OBJECT_TYPE (object) == SEAHORSE_TYPE_GKR_KEYRING) 
		seahorse_gkr_keyring_properties_show (SEAHORSE_GKR_KEYRING (object), window);
	
	else
		g_return_if_reached ();
}

static SeahorseOperation* 
seahorse_gkr_keyring_commands_delete_objects (SeahorseCommands* base, GList* objects) 
{
	gchar *prompt;
	
	if (!objects)
		return NULL;

	prompt = g_strdup_printf (_ ("Are you sure you want to delete the password keyring '%s'?"), 
	                          seahorse_object_get_label (objects->data));

	return seahorse_object_delete (objects->data);
}

static GObject* 
seahorse_gkr_keyring_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_gkr_keyring_commands_parent_class)->constructor (type, n_props, props);
	SeahorseCommands *base = NULL;
	SeahorseView *view;
	
	if (obj) {
		base = SEAHORSE_COMMANDS (obj);
		view = seahorse_commands_get_view (base);
		g_return_val_if_fail (view, NULL);
		seahorse_view_register_commands (view, base, SEAHORSE_TYPE_GKR_KEYRING);
	}
	
	return obj;
}

static void
seahorse_gkr_keyring_commands_init (SeahorseGkrKeyringCommands *self)
{

}

static void
seahorse_gkr_keyring_commands_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_keyring_commands_get_property (GObject *obj, guint prop_id, GValue *value, 
                         	  GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_keyring_commands_class_init (SeahorseGkrKeyringCommandsClass *klass)
{
	GtkActionGroup *actions;
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseCommandsClass *cmd_class = SEAHORSE_COMMANDS_CLASS (klass);
	
	seahorse_gkr_keyring_commands_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructor = seahorse_gkr_keyring_commands_constructor;

	cmd_class->show_properties = seahorse_gkr_keyring_commands_show_properties;
	cmd_class->delete_objects = seahorse_gkr_keyring_commands_delete_objects;

	/* Register this class as a commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_TYPE_GKR_KEYRING_COMMANDS, 
	                                 SEAHORSE_GKR_TYPE_STR, "commands", NULL, NULL);
	
	
	/* Register this as a generator */
	actions = gtk_action_group_new ("gkr-generate");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ACTION_ENTRIES, G_N_ELEMENTS (ACTION_ENTRIES), NULL);
	seahorse_registry_register_object (NULL, G_OBJECT (actions), SEAHORSE_GKR_TYPE_STR, "generator", NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

