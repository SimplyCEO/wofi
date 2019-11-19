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

#define MODE "run"

void wofi_run_init() {
	struct map* cached = map_init();
	struct wl_list* cache = wofi_read_cache(MODE);

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		char* text;
		char* search_prefix;
		char* final_slash = strrchr(node->line, '/');
		if(final_slash == NULL) {
			text = node->line;
			search_prefix = "";
		} else {
			text = final_slash + 1;
			search_prefix = text;
		}
		char* search_text = utils_concat(2, search_prefix, node->line);
		wofi_insert_widget(MODE, &text, search_text, &node->line, 1);
		map_put(cached, node->line, "true");
		free(search_text);
		free(node->line);
		wl_list_remove(&node->link);
		free(node);
	}

	free(cache);

	char* path = strdup(getenv("PATH"));

	char* save_ptr;
	char* str = strtok_r(path, ":", &save_ptr);
	do {
		DIR* dir = opendir(str);
		if(dir == NULL) {
			continue;
		}
		struct dirent* entry;
		while((entry = readdir(dir)) != NULL) {
			if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			char* full_path = utils_concat(3, str, "/", entry->d_name);
			struct stat info;
			stat(full_path, &info);
			if(access(full_path, X_OK) == 0 && S_ISREG(info.st_mode) && !map_contains(cached, full_path)) {
				char* search_text = utils_concat(2, entry->d_name, full_path);
				char* text = strdup(entry->d_name);
				wofi_insert_widget(MODE, &text, search_text, &full_path, 1);
				free(search_text);
				free(text);
			}
			free(full_path);
		}
		closedir(dir);
	} while((str = strtok_r(NULL, ":", &save_ptr)) != NULL);
	free(path);
	map_free(cached);
}

void wofi_run_exec(const gchar* cmd) {
	if(wofi_mod_shift()) {
		wofi_write_cache(MODE, cmd);
		wofi_term_run(cmd);
	}
	if(wofi_mod_control()) {
		char* tmp = strdup(cmd);
		size_t space_count = 2;
		char* space;
		while((space = strchr(tmp, ' ')) != NULL) {
			++space_count;
			*space = '\n';
		}
		char** args = malloc(space_count * sizeof(char*));
		char* save_ptr;
		char* str = strtok_r(tmp, "\n", &save_ptr);
		size_t count = 0;
		do {
			args[count++] = str;
		} while((str = strtok_r(NULL, "\n", &save_ptr)) != NULL);
		args[space_count - 1] = NULL;
		wofi_write_cache(MODE, tmp);
		execvp(tmp, args);
	} else {
		wofi_write_cache(MODE, cmd);
		execl(cmd, cmd, NULL);
	}
	fprintf(stderr, "%s cannot be executed\n", cmd);
	exit(errno);
}
