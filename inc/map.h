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

#ifndef MAP_H
#define MAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct map* map_init();

struct map* map_init_void();

void map_free();

bool map_put(struct map* map, const char* key, char* value);

bool map_put_void(struct map* map, const char* key, void* value);

void* map_get(struct map* map, const char* key);

bool map_contains(struct map* map, const char* key);

size_t map_size(struct map* map);

#endif
