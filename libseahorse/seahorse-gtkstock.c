/**
 * @file seahorse-gtkstock.c GTK+ Stock resources
 *
 * Seahorse
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with the gaim
 * source distribution.
 *
 * Also (c) 2005 Adam Schreiber <sadam@clemson.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <gtk/gtk.h>
 
#include "seahorse-gtkstock.h"

static const gchar *seahorse_icons[] = {
    SEAHORSE_STOCK_KEY,
    SEAHORSE_STOCK_SECRET,
    SEAHORSE_STOCK_KEY_SSH,
    SEAHORSE_STOCK_PERSON,
    SEAHORSE_STOCK_SIGN,
    SEAHORSE_STOCK_SIGN_OK,
    SEAHORSE_STOCK_SIGN_BAD,
    NULL
};

void
seahorse_gtkstock_init(void)
{
    static gboolean stock_initted = FALSE;

    if (stock_initted)
        return;

    stock_initted = TRUE;
    seahorse_gtkstock_add_icons (seahorse_icons);
}

static GtkIconSource*
make_icon_source (const gchar *icon, const gchar *base, const gchar *ext, 
                  GtkIconSize size)
{
    GtkIconSource *source;
    gchar *filename;
    
    filename = g_strdup_printf ("%s/%s/%s.%s", PIXMAPSDIR, base, icon, ext);
    
    source = gtk_icon_source_new ();
    gtk_icon_source_set_filename (source, filename);
    gtk_icon_source_set_direction_wildcarded (source, TRUE);
    gtk_icon_source_set_state_wildcarded (source, TRUE);
    
    if (size == -1) {
        gtk_icon_source_set_size_wildcarded (source, TRUE);
    } else {
        gtk_icon_source_set_size_wildcarded (source, FALSE);
        gtk_icon_source_set_size (source, size);
    }
    
    g_free(filename);
    
    return source;
}

void
seahorse_gtkstock_add_icons (const gchar **icons)
{
    GtkIconFactory *factory;
    GtkIconSource *source;
    GtkIconSet *iconset;
    GtkWidget *win;

    /* Setup the icon factory. */
    factory = gtk_icon_factory_new ();
    gtk_icon_factory_add_default (factory);
    
    /* Er, yeah, a hack, but it works. :) */
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(win);
    
    /* TODO: Implement differently sized icons here */

    for ( ; *icons; icons++) {

        iconset = gtk_icon_set_new ();
        
        source = make_icon_source (*icons, "22x22", "png", GTK_ICON_SIZE_BUTTON);
        gtk_icon_set_add_source (iconset, source);
        gtk_icon_source_free (source);
        source = make_icon_source (*icons, "22x22", "png", GTK_ICON_SIZE_MENU);
        gtk_icon_set_add_source (iconset, source);
        gtk_icon_source_free (source);
        source = make_icon_source (*icons, "22x22", "png", GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_icon_set_add_source (iconset, source);
        gtk_icon_source_free (source);
        source = make_icon_source (*icons, "22x22", "png", GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_icon_set_add_source (iconset, source);
        gtk_icon_source_free (source);
        source = make_icon_source (*icons, "48x48", "png", GTK_ICON_SIZE_DIALOG);
        gtk_icon_set_add_source (iconset, source);
        gtk_icon_source_free (source);
        source = make_icon_source (*icons, "scaleable", "svg", -1);
        gtk_icon_set_add_source (iconset, source);
        gtk_icon_source_free (source);
        
        gtk_icon_factory_add (factory, *icons, iconset);
        gtk_icon_set_unref (iconset);
    }
    
    gtk_widget_destroy (win);
    g_object_unref (factory);
}
