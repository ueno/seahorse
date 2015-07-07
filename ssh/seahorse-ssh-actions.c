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

#include "seahorse-ssh.h"
#include "seahorse-ssh-actions.h"
#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-object.h"
#include "libseahorse/seahorse-object-list.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>
#include <glib.h>
#include <glib-object.h>

GType   seahorse_ssh_actions_get_type             (void);
#define SEAHORSE_TYPE_SSH_ACTIONS                 (seahorse_ssh_actions_get_type ())
#define SEAHORSE_SSH_ACTIONS(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_ACTIONS, SeahorseSshActions))
#define SEAHORSE_SSH_ACTIONS_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_ACTIONS, SeahorseSshActionsClass))
#define SEAHORSE_IS_SSH_ACTIONS(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_ACTIONS))
#define SEAHORSE_IS_SSH_ACTIONS_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_ACTIONS))
#define SEAHORSE_SSH_ACTIONS_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_ACTIONS, SeahorseSshActionsClass))

typedef struct {
	GSimpleActionGroup parent_instance;
} SeahorseSshActions;

typedef struct {
	GSimpleActionGroupClass parent_class;
} SeahorseSshActionsClass;

G_DEFINE_TYPE (SeahorseSshActions, seahorse_ssh_actions, G_TYPE_SIMPLE_ACTION_GROUP);

static void
on_ssh_upload (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GSimpleActionGroup *actions = G_SIMPLE_ACTION_GROUP (user_data);
	SeahorseCatalog *catalog;
	GList *keys, *objects, *l;

	keys = NULL;
	catalog = g_object_get_data (G_OBJECT (actions),
				     "seahorse-action-catalog");

	if (catalog != NULL) {
		objects = seahorse_catalog_get_selected_objects (catalog);
		for (l = objects; l != NULL; l = g_list_next (l)) {
			if (SEAHORSE_IS_SSH_KEY (l->data))
				keys = g_list_prepend (keys, l->data);
		}
		g_list_free (objects);
	}
	g_object_unref (catalog);

	seahorse_ssh_upload_prompt (keys, seahorse_action_get_window (G_ACTION (action)));
	g_list_free (keys);
}

static const GActionEntry KEYS_ACTIONS[] = {
	{ "remote-ssh-upload", on_ssh_upload, NULL, NULL, NULL }
};

static void
seahorse_ssh_actions_init (SeahorseSshActions *self)
{
	g_action_map_add_action_entries (G_ACTION_MAP (self),
					 KEYS_ACTIONS,
					 G_N_ELEMENTS (KEYS_ACTIONS),
					 self);
}

static void
seahorse_ssh_actions_class_init (SeahorseSshActionsClass *klass)
{

}

GActionGroup *
seahorse_ssh_actions_instance (void)
{
	static GActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_SSH_ACTIONS,
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}
