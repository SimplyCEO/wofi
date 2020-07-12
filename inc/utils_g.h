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

#ifndef UTILS_G_H
#define UTILS_G_H

#include <stdint.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

GdkPixbuf* utils_g_resize_pixbuf(GdkPixbuf* pixbuf, uint64_t image_size, GdkInterpType interp);

GdkPixbuf* utils_g_pixbuf_from_base64(char* base64);

#endif
