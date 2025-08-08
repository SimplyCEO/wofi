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

#include <stdbool.h>

#include <map.h>

#include <gtk/gtk.h>

struct widget {
	size_t action_count;
	char* mode, **text, *search_text, **actions;
	struct widget_builder* builder;
};

struct mode {
	void (*mode_exec)(const gchar* cmd);
	struct widget* (*mode_get_widget)(void);
	char* name, *dso;
	struct wl_list link;
};

void wofi_init(struct map* config);

void wofi_load_css(bool nyan);

extern unsigned char input_under_scroll;
extern unsigned char render_only_image;

#endif
