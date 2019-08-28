/*
 *  Copyright (C) 2019 Scoopta
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

#ifndef PROPERTY_LABEL_H
#define PROPERTY_LABEL_H
#include <map.h>

#include <gtk/gtk.h>

#define WOFI_TYPE_PROPERTY_LABEL wofi_property_label_get_type()
G_DECLARE_FINAL_TYPE(WofiPropertyLabel, wofi_property_label, WOFI, PROPERTY_LABEL, GtkLabel);

GtkWidget* wofi_property_label_new(const gchar* str);

void wofi_property_label_add_property(WofiPropertyLabel* this, const gchar* key, gchar* value);

const gchar* wofi_property_label_get_property(WofiPropertyLabel* this, const gchar* key);

#endif
