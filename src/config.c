/*
 *  Copyright (C) 2019-2024 Scoopta
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include <map.h>

void config_put(struct map* map, char* line) {
	size_t line_size = strlen(line);
	char* new_line = calloc(1, line_size + 1);
	size_t new_line_count = 0;
	for(size_t count = 0; count < line_size; ++count) {
		if(line[count] == '\\') {
			new_line[new_line_count++] = line[++count];
		} else if(line[count] == '#') {
			break;
		} else {
			new_line[new_line_count++] = line[count];
		}
	}
	line = new_line;
	char* equals = strchr(line, '=');
	if(equals == NULL) {
		free(line);
		return;
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
	char* end = value + len - 1;
	if(*end == '\n' || *end == ' ') {
		*end = 0;
	}
	map_put(map, line, value);
	free(line);
}

void config_load(struct map* map, const char* config) {
	FILE* file = fopen(config, "r");
	char* line = NULL;
	size_t size = 0;
	while(getline(&line, &size, file) != -1) {
		config_put(map, line);
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

int config_get_mnemonic(struct map* config, const char* key, char* def_opt, int num_choices, ...) {
	char* opt = config_get(config, key, def_opt);
	va_list ap;
	va_start(ap, num_choices);
	int result = 0;
	for(int i = 0; i < num_choices; i++) {
		char* cmp_str = va_arg(ap, char*);
		if(strcmp(opt, cmp_str) == 0) {
			result = i;
			break;
		}
	}
	va_end(ap);
	return result;
}
