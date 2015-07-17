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

#ifndef __SEAHORSE_PANED_H__
#define __SEAHORSE_PANED_H__

#include "seahorse-common.h"

G_BEGIN_DECLS

#define SEAHORSE_TYPE_PANED seahorse_paned_get_type ()
G_DECLARE_FINAL_TYPE (SeahorsePaned, seahorse_paned, SEAHORSE, PANED, GtkPaned);

SeahorsePaned *seahorse_paned_new (GList *backends);
SeahorsePlace *seahorse_paned_get_focused_place (SeahorsePaned *self);
GList *seahorse_paned_get_selected_objects (SeahorsePaned *self);
void seahorse_paned_set_filter_flags (SeahorsePaned *self, SeahorseFlags flags);
void seahorse_paned_set_filter_text (SeahorsePaned *self, const gchar *text);

G_END_DECLS

#endif	/* __SEAHORSE_PANED_H__ */
