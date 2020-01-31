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

#ifndef WOFI_API_H
#define WOFI_API_H

#include <map.h>

#include <stdint.h>
#include <stdbool.h>

#include <wayland-client.h>

struct cache_line {
	char* line;
	struct wl_list link;
};

struct mode;

char* wofi_parse_image_escapes(const char* text);

void wofi_write_cache(struct mode* mode, const char* cmd);

void wofi_remove_cache(struct mode* mode, const char* cmd);

struct wl_list* wofi_read_cache(struct mode* mode);

struct widget* wofi_create_widget(struct mode* mode, char** text, char* search_text, char** actions, size_t action_count);

void wofi_insert_widgets(struct mode* mode);

bool wofi_allow_images(void);

bool wofi_allow_markup(void);

uint64_t wofi_get_image_size(void);

bool wofi_mod_shift(void);

bool wofi_mod_control(void);

void wofi_term_run(const char* cmd);

#endif
