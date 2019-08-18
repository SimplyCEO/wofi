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

#include <config.h>

void config_load(struct map* map, const char* config) {
	FILE* file = fopen(config, "r");
	char* line = NULL;
	size_t size = 0;
	while(getline(&line, &size, file) != -1) {
		char* hash = strchr(line, '#');
		if(hash != NULL) {
			if(hash == line || *(hash - 1) != '\\') {
				*hash = 0;
			}
		}
		char* backslash = strchr(line, '\\');
		size_t backslash_count = 0;
		while(backslash != NULL) {
			++backslash_count;
			backslash = strchr(backslash + 1, '\\');
		}
		char* new_line = calloc(1, size - backslash_count);
		size_t line_size = strlen(line);
		size_t new_line_count = 0;
		for(size_t count = 0; count < line_size; ++count) {
			if(line[count] == '\\') {
				continue;
			}
			new_line[new_line_count++] = line[count];
		}
		free(line);
		line = new_line;
		char* equals = strchr(line, '=');
		if(equals == NULL) {
			continue;
		}
		*equals = 0;
		char* key = equals - 1;
		while(*key == ' ') {
			--key;
		}
		*(key + 1) = 0;
		char* value = equals + 1;
		while(*value == ' ') {
			++value;
		}
		size_t len = strlen(value);
		*(value + len - 1) = 0;
		map_put(map, line, value);
	}
	free(line);
	fclose(file);
}

char* config_get(struct map* config, const char* key, char* def_opt) {
	char* opt = map_get(config, key);
	if(opt == NULL) {
		opt = def_opt;
	}
	return opt;
}
