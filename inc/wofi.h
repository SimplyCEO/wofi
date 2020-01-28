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

#ifndef WAIFU_H
#define WAIFU_H

#include <wofi_api.h>

#include <config.h>
#include <property_box.h>

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include <sys/stat.h>

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <gdk/gdkwayland.h>

#include <wlr-layer-shell-unstable-v1-client-protocol.h>

void wofi_init(struct map* config);
#endif
