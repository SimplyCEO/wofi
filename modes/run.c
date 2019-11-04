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

void wofi_run_init() {
	struct map* cached = map_init();
	struct wl_list* cache = wofi_read_cache("run");

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		char* text = strrchr(node->line, '/') + 1;
		char* search_text = utils_concat(2, text, node->line);
		wofi_insert_widget(text, search_text, node->line);
		map_put(cached, node->line, "true");
		free(search_text);
		free(node->line);
		wl_list_remove(&node->link);
		free(node);
	}

	free(cache);

	char* path = strdup(getenv("PATH"));
	char* original_path = path;
	size_t colon_count = utils_split(path, ':');
	for(size_t count = 0; count < colon_count; ++count) {
		DIR* dir = opendir(path);
		if(dir == NULL) {
			goto cont;
		}
		struct dirent* entry;
		while((entry = readdir(dir)) != NULL) {
			if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			char* full_path = utils_concat(3, path, "/", entry->d_name);
			struct stat info;
			stat(full_path, &info);
			if(access(full_path, X_OK) == 0 && S_ISREG(info.st_mode) && !map_contains(cached, full_path)) {
				char* search_text = utils_concat(2, entry->d_name, full_path);
				wofi_insert_widget(entry->d_name, search_text, full_path);
				free(search_text);
			}
			free(full_path);
		}
		closedir(dir);
		cont:
		path += strlen(path) + 1;
	}
	free(original_path);
	map_free(cached);
}

void wofi_run_exec(const gchar* cmd) {
	if(wofi_run_in_term()) {
		wofi_term_run(cmd);
	}
	execl(cmd, cmd, NULL);
	fprintf(stderr, "%s cannot be executed\n", cmd);
	exit(errno);
}
