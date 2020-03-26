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

#include <property_box.h>

#include <map.h>

struct _WofiPropertyBox {
	GtkBox super;
};

typedef struct {
	struct map* properties;
} WofiPropertyBoxPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(WofiPropertyBox, wofi_property_box, GTK_TYPE_BOX);

static void wofi_property_box_init(WofiPropertyBox* box) {
	WofiPropertyBoxPrivate* this = wofi_property_box_get_instance_private(box);
	this->properties = map_init();
}

static void finalize(GObject* obj) {
	WofiPropertyBoxPrivate* this = wofi_property_box_get_instance_private(WOFI_PROPERTY_BOX(obj));
	map_free(this->properties);
	G_OBJECT_CLASS(wofi_property_box_parent_class)->finalize(obj);
}

static void wofi_property_box_class_init(WofiPropertyBoxClass* class) {
	GObjectClass* g_class = G_OBJECT_CLASS(class);
	g_class->finalize = finalize;
}

GtkWidget* wofi_property_box_new(GtkOrientation orientation, gint spacing) {
	return g_object_new(WOFI_TYPE_PROPERTY_BOX, "orientation", orientation, "spacing", spacing, NULL);
}

void wofi_property_box_add_property(WofiPropertyBox* box, const gchar* key, gchar* value) {
	WofiPropertyBoxPrivate* this = wofi_property_box_get_instance_private(box);
	map_put(this->properties, key, value);
}

const gchar* wofi_property_box_get_property(WofiPropertyBox* box, const gchar* key) {
	WofiPropertyBoxPrivate* this = wofi_property_box_get_instance_private(box);
	return map_get(this->properties, key);
}
