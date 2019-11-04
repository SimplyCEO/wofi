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

#include <wofi.h>

void wofi_dmenu_init() {
	struct map* cached = map_init();
	struct wl_list* cache = wofi_read_cache("dmenu");

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		wofi_insert_widget(node->line, node->line, node->line);
		map_put(cached, node->line, "true");
		free(node->line);
		wl_list_remove(&node->link);
		free(node);
	}

	free(cache);

	char* line;
	size_t size = 0;
	while(getline(&line, &size, stdin) != -1) {
		char* lf = strchr(line, '\n');
		if(lf != NULL) {
			*lf = 0;
		}
		if(map_contains(cached, line)) {
			continue;
		}
		wofi_insert_widget(line, line, line);
	}
	free(line);
	map_free(cached);
}

void wofi_dmenu_exec(const gchar* cmd) {
	printf("%s\n", cmd);
	exit(0);
}

bool wofi_dmenu_exec_search() {
	return true;
}
