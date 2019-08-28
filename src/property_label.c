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

#include <property_label.h>

struct _WofiPropertyLabel {
	GtkLabel super;
	struct map* properties;
};

G_DEFINE_TYPE(WofiPropertyLabel, wofi_property_label, GTK_TYPE_LABEL);

static void wofi_property_label_init(WofiPropertyLabel* this) {
	this->properties = map_init();
}

static void finalize(GObject* obj) {
	WofiPropertyLabel* this = WOFI_PROPERTY_LABEL(obj);
	map_free(this->properties);
	G_OBJECT_CLASS(wofi_property_label_parent_class)->finalize(obj);
}

static void wofi_property_label_class_init(WofiPropertyLabelClass* class) {
	GObjectClass* g_class = G_OBJECT_CLASS(class);
	g_class->finalize = finalize;
}

GtkWidget* wofi_property_label_new(const gchar* str) {
	return g_object_new(WOFI_TYPE_PROPERTY_LABEL, "label", str, NULL);
}

void wofi_property_label_add_property(WofiPropertyLabel* this, const gchar* key, gchar* value) {
	map_put(this->properties, key, value);
}

const gchar* wofi_property_label_get_property(WofiPropertyLabel* this, const gchar* key) {
	return map_get(this->properties, key);
}
