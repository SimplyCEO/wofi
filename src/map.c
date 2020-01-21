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

#include <map.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gmodule.h>

struct map {
	GTree* tree;
	bool mman;
};

static gint compare(gconstpointer p1, gconstpointer p2, gpointer data) {
	(void) data;
	const char* str1 = p1;
	const char* str2 = p2;
	return strcmp(str1, str2);
}

struct map* map_init(void) {
	struct map* map = malloc(sizeof(struct map));
	map->tree = g_tree_new_full(compare, NULL, free, free);
	map->mman = true;
	return map;
}

struct map* map_init_void(void) {
	struct map* map = malloc(sizeof(struct map));
	map->tree = g_tree_new_full(compare, NULL, free, NULL);
	map->mman = false;
	return map;
}

void map_free(struct map* map) {
	g_tree_destroy(map->tree);
	free(map);
}

static void put(struct map* map, const char* key, void* value) {
	char* k = strdup(key);
	char* v = value;
	if(map->mman && value != NULL) {
		v = strdup(value);
	}
	g_tree_insert(map->tree, k, v);
}

bool map_put(struct map* map, const char* key, char* value) {
	if(map->mman) {
		put(map, key, value);
		return true;
	} else {
		fprintf(stderr, "This is an unmanaged map please use map_put_void\n");
		return false;
	}
}

bool map_put_void(struct map* map, const char* key, void* value) {
	if(map->mman) {
		fprintf(stderr, "This is an managed map please use map_put\n");
		return false;
	} else {
		put(map, key, value);
		return true;
	}
}

void* map_get(struct map* map, const char* key) {
	return g_tree_lookup(map->tree, key);
}

bool map_contains(struct map* map, const char* key) {
	return map_get(map, key) != NULL;
}

size_t map_size(struct map* map) {
	return g_tree_nnodes(map->tree);
}
