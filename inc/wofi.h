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

#ifndef WAIFU_H
#define WAIFU_H

#include <map.h>
#include <config.h>
#include <property_box.h>

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <sys/stat.h>

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gio/gdesktopappinfo.h>

#include <wayland-client.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>

struct cache_line {
	char* line;
	struct wl_list link;
};

void wofi_init(struct map* config);

void wofi_write_cache(const gchar* mode, const gchar* cmd);

struct wl_list* wofi_read_cache(char* mode);

void wofi_insert_widget(char* mode, char** text, char* search_text, char** actions, size_t action_count);

bool wofi_allow_images();

uint64_t wofi_get_image_size();

bool wofi_run_in_term();

bool wofi_mod_control();

void wofi_term_run(const char* cmd);
#endif
