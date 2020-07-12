/*
 *  Copyright (C) 2020 Scoopta
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

#include <utils_g.h>

#include <wofi_api.h>

GdkPixbuf* utils_g_resize_pixbuf(GdkPixbuf* pixbuf, uint64_t image_size, GdkInterpType interp) {
	int width = gdk_pixbuf_get_width(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);

	if(height > width) {
		float percent = (float) image_size / height;
		GdkPixbuf* tmp = gdk_pixbuf_scale_simple(pixbuf, width * percent, image_size, interp);
		g_object_unref(pixbuf);
		return tmp;
	} else {
		float percent = (float) image_size / width;
		GdkPixbuf* tmp = gdk_pixbuf_scale_simple(pixbuf, image_size, height * percent, interp);
		g_object_unref(pixbuf);
		return tmp;
	}
}

GdkPixbuf* utils_g_pixbuf_from_base64(char* base64) {
	char* str = strdup(base64);
	char* original_str = str;

	GError* err = NULL;
	GdkPixbufLoader* loader;
	if(strncmp(str, "image/", sizeof("image/") - 1) == 0) {
		char* mime = strchr(str, ';');
		*mime = 0;
		loader = gdk_pixbuf_loader_new_with_mime_type(str, &err);
		if(err != NULL) {
			goto fail;
		}
		str = mime + 1;
		str = strchr(str, ',') + 1;
	} else {
		loader = gdk_pixbuf_loader_new();
	}

	gsize data_l;
	guchar* data = g_base64_decode(str, &data_l);

	gdk_pixbuf_loader_write(loader, data, data_l, &err);
	if(err != NULL) {
		g_free(data);
		goto fail;
	}

	g_free(data);

	free(original_str);
	return gdk_pixbuf_loader_get_pixbuf(loader);

	fail:
	free(str);
	fprintf(stderr, "Error loading base64 %s\n", err->message);
	return NULL;
}
