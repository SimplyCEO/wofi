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

static const char* arg_names[] = {"always_parse_args", "show_all"};

static bool always_parse_args;
static bool show_all;

void wofi_run_init(struct map* config) {
	always_parse_args = strcmp(config_get(config, arg_names[0], "false"), "true") == 0;
	show_all = strcmp(config_get(config, arg_names[1], "true"), "true") == 0;

	struct map* cached = map_init();
	struct wl_list* cache = wofi_read_cache(MODE);

	struct map* entries = map_init();

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		char* text;
		char* final_slash = strrchr(node->line, '/');
		if(final_slash == NULL) {
			text = node->line;
		} else {
			text = final_slash + 1;
		}
		struct stat info;
		stat(node->line, &info);
		if(access(node->line, X_OK) == 0 && S_ISREG(info.st_mode)) {
			wofi_insert_widget(MODE, &text, text, &node->line, 1);
			map_put(cached, node->line, "true");
			map_put(entries, text, "true");
		} else {
			wofi_remove_cache(MODE, node->line);
		}
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
			if(access(full_path, X_OK) == 0 && S_ISREG(info.st_mode) && !map_contains(cached, full_path) && (show_all || !map_contains(entries, entry->d_name))) {
				char* text = strdup(entry->d_name);
				map_put(entries, text, "true");
				wofi_insert_widget(MODE, &text, text, &full_path, 1);
				free(text);
			}
			free(full_path);
		}
		closedir(dir);
	} while((str = strtok_r(NULL, ":", &save_ptr)) != NULL);
	free(path);
	map_free(cached);
	map_free(entries);
}

void wofi_run_exec(const gchar* cmd) {
	if(wofi_mod_shift()) {
		wofi_write_cache(MODE, cmd);
		wofi_term_run(cmd);
	}
	if(wofi_mod_control() || always_parse_args) {
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

const char** wofi_run_get_arg_names(void) {
	return arg_names;
}

size_t wofi_run_get_arg_count(void) {
	return sizeof(arg_names) / sizeof(char*);
}
