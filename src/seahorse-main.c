/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004,2005 Stefan Walter
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

#include "seahorse-key-manager.h"

#include "seahorse-common.h"
#include "seahorse-resources.h"

#include "libseahorse/seahorse-application.h"
#include "libseahorse/seahorse-prefs.h"
#include "libseahorse/seahorse-servers.h"
#include "libseahorse/seahorse-util.h"

#include "pgp/seahorse-pgp.h"
#include "ssh/seahorse-ssh.h"

#include <glib/gi18n.h>

#include <locale.h>
#include <stdlib.h>

static void
on_app_preferences (GSimpleAction *action, GVariant *parameter,
		    gpointer user_data)
{
	GtkApplication *application = GTK_APPLICATION (user_data);
	GtkWindow *window = gtk_application_get_active_window (application);
	seahorse_prefs_show (window, NULL);
}

static void
on_app_help (GSimpleAction *action, GVariant *parameter,
	     gpointer user_data)
{
	GtkApplication *application = GTK_APPLICATION (user_data);
	GtkWindow *window = gtk_application_get_active_window (application);
	GError *error = NULL;
	if (!g_app_info_launch_default_for_uri ("help:" PACKAGE_NAME, NULL,
						&error)) {
		seahorse_util_show_error (GTK_WIDGET (window),
					  _("Could not display help: %s"),
					  error->message);
		g_error_free (error);
	}
}

static void
on_app_about (GSimpleAction *action, GVariant *parameter,
	      gpointer user_data)
{
	GtkApplication *application = GTK_APPLICATION (user_data);
	GtkWindow *window = gtk_application_get_active_window (application);

	const gchar *authors[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
		"Jean Schurger <yshark@schurger.org>",
		"Stef Walter <stef@memberwebs.com>",
		"Adam Schreiber <sadam@clemson.edu>",
		"",
		N_("Contributions:"),
		"Albrecht Dreß <albrecht.dress@arcor.de>",
		"Jim Pharis <binbrain@gmail.com>",
		NULL
	};

	const gchar *documentors[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Adam Schreiber <sadam@clemson.edu>",
		"Milo Casagrande <milo_casagrande@yahoo.it>",
		NULL
	};

	const gchar *artists[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Stef Walter <stef@memberwebs.com>",
		NULL
	};

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "version", PACKAGE_VERSION,
			       "copyright", "\xc2\xa9 2002 - 2010 Seahorse Project",
			       "website", "http://www.gnome.org/projects/seahorse",
			       "comments", _("Passwords and Keys"),
			       "authors", authors,
			       "documenters", documentors,
			       "artists", artists,
			       "logo-icon-name", "seahorse",
			       NULL);
}

static void
on_app_quit (GSimpleAction *action, GVariant *parameter,
             gpointer user_data)
{
	g_application_quit (G_APPLICATION (seahorse_application_get ()));
}

static const GActionEntry APPLICATION_ACTIONS[] = {
	{ "preferences", on_app_preferences, NULL, NULL, NULL },
	{ "help", on_app_help, NULL, NULL, NULL },
	{ "about", on_app_about, NULL, NULL, NULL },
	{ "quit", on_app_quit, NULL, NULL, NULL }
};

static void
on_application_activate (GApplication *application,
                         gpointer user_data)
{
	seahorse_key_manager_show (GDK_CURRENT_TIME);
}

#ifdef WITH_PKCS11
void seahorse_pkcs11_backend_initialize (void);
#endif

void seahorse_gkr_backend_initialize (void);

static void
on_application_startup (GApplication *application,
                        gpointer user_data)
{
	GtkBuilder *builder;
	GMenuModel *appmenu;

	/* Initialize the various components */
#ifdef WITH_PGP
	seahorse_pgp_backend_initialize ();
#endif
#ifdef WITH_SSH
	seahorse_ssh_backend_initialize ();
#endif
#ifdef WITH_PKCS11
	seahorse_pkcs11_backend_initialize ();
#endif
	seahorse_gkr_backend_initialize ();

	/* Initialize the search provider now that backends are registered */
	seahorse_application_initialize_search (SEAHORSE_APPLICATION (application));

	/* Initialize the application menu */
	builder = gtk_builder_new_from_resource ("/org/gnome/Seahorse/seahorse-key-manager.xml");
	appmenu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));
	gtk_application_set_app_menu (GTK_APPLICATION (application), appmenu);
	g_object_unref (builder);
}

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
	GtkApplication *application;
	int status;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

#if !GLIB_CHECK_VERSION(2,35,0)
	g_type_init ();
#endif

	seahorse_register_resource ();

	application = seahorse_application_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (application),
					 APPLICATION_ACTIONS, G_N_ELEMENTS (APPLICATION_ACTIONS),
					 application);
	g_signal_connect (application, "activate", G_CALLBACK (on_application_activate), NULL);
	g_signal_connect (application, "startup", G_CALLBACK (on_application_startup), NULL);
	status = g_application_run (G_APPLICATION (application), argc, argv);

	seahorse_registry_cleanup ();
	seahorse_servers_cleanup ();
	g_object_unref (application);

	return status;
}
