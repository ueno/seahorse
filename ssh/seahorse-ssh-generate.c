/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
 
#include <glib/gi18n.h>

#include "seahorse-ssh-backend.h"
#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-source.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"
#include "libseahorse/seahorse-widget.h"


enum {
	PROP_0,
	PROP_NAME,
	PROP_ACTIONS,
	PROP_MENU
};

#define SEAHORSE_TYPE_SSH_GENERATOR (seahorse_ssh_generator_get_type ())

G_DECLARE_FINAL_TYPE (SeahorseSshGenerator, seahorse_ssh_generator,
		      SEAHORSE, SSH_GENERATOR, GObject);

struct _SeahorseSshGenerator {
	GObject parent;
	GActionGroup *actions;
	GMenuModel *menu;
};

struct _SeahorseSshGeneratorClass {
	GObjectClass parent_class;
};

static void seahorse_ssh_generator_iface (SeahorseGeneratorIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseSshGenerator, seahorse_ssh_generator, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_GENERATOR, seahorse_ssh_generator_iface);
);

static void
seahorse_ssh_generator_init (SeahorseSshGenerator *self)
{
}

/* --------------------------------------------------------------------------
 * ACTIONS
 */

static void
on_ssh_generate_key (GSimpleAction *action, GVariant *parameter,
		     gpointer unused)
{
	seahorse_ssh_generate_show (seahorse_ssh_backend_get_dot_ssh (NULL),
	                            seahorse_action_get_window (G_ACTION (action)));
}

static const GActionEntry ACTION_ENTRIES[] = {
	{ "ssh-generate-key", on_ssh_generate_key, NULL, NULL, NULL }
};

static void
seahorse_ssh_generator_constructed (GObject *obj)
{
	SeahorseSshGenerator *self = SEAHORSE_SSH_GENERATOR (obj);
	GMenuItem *item;
	GIcon *icon;

	G_OBJECT_CLASS (seahorse_ssh_generator_parent_class)->constructed (obj);

	self->actions = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
					 ACTION_ENTRIES,
					 G_N_ELEMENTS (ACTION_ENTRIES),
					 self);

	self->menu = G_MENU_MODEL (g_menu_new ());
	item = g_menu_item_new (_("Secure Shell Key"),
				SEAHORSE_SSH_NAME ".ssh-generate-key");
	icon = g_themed_icon_new (GCR_ICON_KEY_PAIR);
	g_menu_item_set_icon (item, icon);
	g_object_unref (icon);
	g_menu_item_set_attribute (item, "tooltip", "s",
				   N_("Used to access other computers (eg: via a terminal)"));
	g_menu_append_item (G_MENU (self->menu), item);
	g_object_unref (item);
}

static const gchar *
seahorse_ssh_generator_get_name (SeahorseGenerator *generator)
{
	return SEAHORSE_SSH_NAME;
}

static GActionGroup *
seahorse_ssh_generator_get_actions (SeahorseGenerator *generator)
{
	SeahorseSshGenerator *self = SEAHORSE_SSH_GENERATOR (generator);
	return g_object_ref (self->actions);
}

static GMenuModel *
seahorse_ssh_generator_get_menu (SeahorseGenerator *generator)
{
	SeahorseSshGenerator *self = SEAHORSE_SSH_GENERATOR (generator);
	return g_object_ref (self->menu);
}

static void
seahorse_ssh_generator_get_property (GObject *obj,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	SeahorseGenerator *generator = SEAHORSE_GENERATOR (obj);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, seahorse_ssh_generator_get_name (generator));
		break;
	case PROP_ACTIONS:
		g_value_take_object (value, seahorse_ssh_generator_get_actions (generator));
		break;
	case PROP_MENU:
		g_value_take_object (value, seahorse_ssh_generator_get_menu (generator));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_ssh_generator_finalize (GObject *obj)
{
	SeahorseSshGenerator *self = SEAHORSE_SSH_GENERATOR (obj);

	g_clear_object (&self->actions);
	g_clear_object (&self->menu);

	G_OBJECT_CLASS (seahorse_ssh_generator_parent_class)->finalize (obj);
}

static void
seahorse_ssh_generator_class_init (SeahorseSshGeneratorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_ssh_generator_constructed;
	gobject_class->get_property = seahorse_ssh_generator_get_property;
	gobject_class->finalize = seahorse_ssh_generator_finalize;

	g_object_class_override_property (gobject_class, PROP_NAME, "name");
	g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
	g_object_class_override_property (gobject_class, PROP_MENU, "menu");
}

static void
seahorse_ssh_generator_iface (SeahorseGeneratorIface *iface)
{
	iface->get_actions = seahorse_ssh_generator_get_actions;
	iface->get_menu = seahorse_ssh_generator_get_menu;
	iface->get_name = seahorse_ssh_generator_get_name;
}

void
seahorse_ssh_generate_register (void)
{
	SeahorseSshGenerator *generator = g_object_new (SEAHORSE_TYPE_SSH_GENERATOR, NULL);
	seahorse_generator_register (SEAHORSE_GENERATOR (generator));
	g_object_unref (generator);
}

/* --------------------------------------------------------------------
 * DIALOGS
 */

#define DSA_SIZE 1024
#define DEFAULT_RSA_SIZE 2048

static void
on_change (GtkComboBox *combo, SeahorseWidget *swidget)
{
    const gchar *t;    
    GtkWidget *widget;
    
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);

    t = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
    if (t && strstr (t, "DSA")) {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), DSA_SIZE);
        gtk_widget_set_sensitive (widget, FALSE);
    } else {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), DEFAULT_RSA_SIZE);
        gtk_widget_set_sensitive (widget, TRUE);
    }
}

static void
on_generate_complete (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GError *error = NULL;

	seahorse_ssh_op_generate_finish (SEAHORSE_SSH_SOURCE (source),
	                                 result, &error);

	if (error != NULL)
		seahorse_util_handle_error (&error, NULL, _("Couldn't generate Secure Shell key"));
}

static void
on_generate_complete_and_upload (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GError *error = NULL;
	SeahorseObject *object;
	GList *keys;

	object = seahorse_ssh_op_generate_finish (SEAHORSE_SSH_SOURCE (source),
	                                          result, &error);

	if (error != NULL) {
		seahorse_util_handle_error (&error, NULL, _("Couldn't generate Secure Shell key"));

	} else {
		keys = g_list_append (NULL, object);
		seahorse_ssh_upload_prompt (keys, NULL);
		g_list_free (keys);
	}
}

static void
on_response (GtkDialog *dialog, gint response, SeahorseWidget *swidget)
{
    SeahorseSSHSource *src;
    GCancellable *cancellable;
    GtkWidget *widget;
    const gchar *email;
    const gchar *t;
    gboolean upload;
    guint type;
    guint bits;
    
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help (swidget);
        return;
    }
    
    if (response == GTK_RESPONSE_OK) 
        upload = TRUE;
    else if (response == GTK_RESPONSE_CLOSE)
        upload = FALSE;
    else {
        seahorse_widget_destroy (swidget);
        return;
    }
    
    /* The email address */
    widget = seahorse_widget_get_widget (swidget, "email-entry");
    g_return_if_fail (widget != NULL);
    email = gtk_entry_get_text (GTK_ENTRY (widget));
    
    /* The algorithm */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    t = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));
    if (t && strstr (t, "DSA"))
        type = SSH_ALGO_DSA;
    else
        type = SSH_ALGO_RSA;
    
    /* The number of bits */
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);
    bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
    if (bits < 512 || bits > 8192) {
        g_message ("invalid key size: %s defaulting to 2048", t);
        bits = 2048;
    }
    
    src = SEAHORSE_SSH_SOURCE (g_object_get_data (G_OBJECT (swidget), "source"));
    g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (src));

    /* We start creation */
    cancellable = g_cancellable_new ();
    seahorse_ssh_op_generate_async (src, email, type, bits,
                                    gtk_window_get_transient_for (GTK_WINDOW (dialog)), cancellable,
                                    upload ? on_generate_complete_and_upload : on_generate_complete,
                                    NULL);
    seahorse_progress_show (cancellable, _("Creating Secure Shell Key"), FALSE);
    g_object_unref (cancellable);

    seahorse_widget_destroy (swidget);
}

void
seahorse_ssh_generate_show (SeahorseSSHSource *src, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;
    
    swidget = seahorse_widget_new ("ssh-generate", parent);
    
    /* Widget already present */
    if (swidget == NULL)
        return;

    g_object_ref (src);
    g_object_set_data_full (G_OBJECT (swidget), "source", src, g_object_unref);

    g_signal_connect (G_OBJECT (seahorse_widget_get_widget (swidget, "algorithm-choice")), "changed", 
                    G_CALLBACK (on_change), swidget);

    g_signal_connect (seahorse_widget_get_toplevel (swidget), "response", 
                    G_CALLBACK (on_response), swidget);

    /* on_change() gets called, bits entry is setup */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
}
