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

#include <map.h>

struct map {
	char* key;
	void* value;
	size_t size;
	bool mman;
	struct map* head, *left, *right;
};

struct map* map_init() {
	struct map* map = calloc(1, sizeof(struct map));
	map->head = map;
	map->mman = true;
	return map;
}

struct map* map_init_void() {
	struct map* map = map_init();
	map->mman = false;
	return map;
}

void map_free(struct map* map) {
	if(map->left != NULL) {
		map_free(map->left);
	}
	if(map->right != NULL) {
		map_free(map->right);
	}
	if(map->key != NULL) {
		free(map->key);
	}
	if(map->value != NULL && map->head->mman) {
		free(map->value);
	}
	free(map);
}

static void put(struct map* map, const char* key, void* value) {
	if(map->key == NULL) {
		map->key = strdup(key);
		if(value != NULL && map->head->mman) {
			map->value = strdup(value);
		} else {
			map->value = value;
		}
		++map->head->size;
	} else if(strcmp(key, map->key) < 0) {
		if(map->left == NULL) {
			map->left = map_init();
			map->left->head = map->head;
		}
		put(map->left, key, value);
	} else if(strcmp(key, map->key) > 0) {
		if(map->right == NULL) {
			map->right = map_init();
			map->right->head = map->head;
		}
		put(map->right, key, value);
	} else {
		if(map->value != NULL && map->head->mman) {
			free(map->value);
		}
		if(value != NULL && map->head->mman) {
			map->value = strdup(value);
		} else {
			map->value = value;
		}
	}
}

bool map_put(struct map* map, const char* key, char* value) {
	if(map->head->mman) {
		put(map, key, value);
		return true;
	} else {
		fprintf(stderr, "This is an unmanaged map please use map_put_void\n");
		return false;
	}
}

bool map_put_void(struct map* map, const char* key, void* value) {
	if(map->head->mman) {
		fprintf(stderr, "This is an managed map please use map_put\n");
		return false;
	} else {
		put(map, key, value);
		return true;
	}
}

void* map_get(struct map* map, const char* key) {
	if(map->key == NULL) {
		return NULL;
	} else if(strcmp(key, map->key) < 0) {
		if(map->left == NULL) {
			return NULL;
		}
		return map_get(map->left, key);
	} else if(strcmp(key, map->key) > 0) {
		if(map->right == NULL) {
			return NULL;
		}
		return map_get(map->right, key);
	} else {
		return map->value;
	}
}

bool map_contains(struct map* map, const char* key) {
	return map_get(map, key) != NULL;
}

size_t map_size(struct map* map) {
	return map->size;
}
