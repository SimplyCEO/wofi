/*
 *  Copyright (C) 2019-2020 Scoopta
 *  This file is part of Wofi
 *  Wofi is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Wofi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Wofi.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PROPERTY_BOX_H
#define PROPERTY_BOX_H

#include <gtk/gtk.h>


#define WOFI_TYPE_PROPERTY_BOX wofi_property_box_get_type()
G_DECLARE_FINAL_TYPE(WofiPropertyBox, wofi_property_box, WOFI, PROPERTY_BOX, GtkBox);

GtkWidget* wofi_property_box_new(GtkOrientation orientation, gint spacing);

void wofi_property_box_add_property(WofiPropertyBox* this, const gchar* key, gchar* value);

const gchar* wofi_property_box_get_property(WofiPropertyBox* this, const gchar* key);

#endif
