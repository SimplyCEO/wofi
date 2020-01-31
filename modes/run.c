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

#include <wofi.h>

static const char* arg_names[] = {"always_parse_args", "show_all"};

static bool always_parse_args;
static bool show_all;
static struct mode* mode;
static const char* arg_str = "__args";

struct node {
	struct widget* widget;
	struct wl_list link;
};

static struct wl_list widgets;

void wofi_run_init(struct mode* this, struct map* config) {
	mode = this;
	always_parse_args = strcmp(config_get(config, arg_names[0], "false"), "true") == 0;
	show_all = strcmp(config_get(config, arg_names[1], "true"), "true") == 0;

	wl_list_init(&widgets);

	struct map* cached = map_init();
	struct wl_list* cache = wofi_read_cache(mode);

	struct map* entries = map_init();

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		char* text = node->line;
		if(strncmp(node->line, arg_str, strlen(arg_str)) == 0) {
			text = strchr(text, ' ') + 1;
		}

		char* full_path = text;

		char* final_slash = strrchr(text, '/');
		if(final_slash != NULL) {
			text = final_slash + 1;
		}

		struct stat info;
		stat(node->line, &info);
		if(((access(node->line, X_OK) == 0 && S_ISREG(info.st_mode)) ||
				strncmp(node->line, arg_str, strlen(arg_str)) == 0) && !map_contains(cached, full_path)) {
			struct node* widget = malloc(sizeof(struct node));
			widget->widget = wofi_create_widget(mode, &text, text, &node->line, 1);
			wl_list_insert(&widgets, &widget->link);
			map_put(cached, full_path, "true");
			map_put(entries, text, "true");
		} else {
			wofi_remove_cache(mode, node->line);
		}
		free(node->line);
		wl_list_remove(&node->link);
		free(node);
	}

	free(cache);

	char* path = strdup(getenv("PATH"));

	struct map* paths = map_init();

	char* save_ptr;
	char* str = strtok_r(path, ":", &save_ptr);
	do {

		str = realpath(str, NULL);
		if(str == NULL) {
			continue;
		}
		if(map_contains(paths, str)) {
			free(str);
			continue;
		}

		map_put(paths, str, "true");

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
			if(access(full_path, X_OK) == 0 && S_ISREG(info.st_mode) &&
					!map_contains(cached, full_path) &&
					(show_all || !map_contains(entries, entry->d_name))) {
				char* text = strdup(entry->d_name);
				map_put(entries, text, "true");
				struct node* widget = malloc(sizeof(struct node));
				widget->widget = wofi_create_widget(mode, &text, text, &full_path, 1);
				wl_list_insert(&widgets, &widget->link);
				free(text);
			}
			free(full_path);
		}
		free(str);
		closedir(dir);
	} while((str = strtok_r(NULL, ":", &save_ptr)) != NULL);
	free(path);
	map_free(paths);
	map_free(cached);
	map_free(entries);
}

struct widget* wofi_run_get_widget(void) {
	struct node* node, *tmp;
	wl_list_for_each_reverse_safe(node, tmp, &widgets, link) {
		struct widget* widget = node->widget;
		wl_list_remove(&node->link);
		free(node);
		return widget;
	}
	return NULL;
}


void wofi_run_exec(const gchar* cmd) {
	bool arg_run = wofi_mod_control() || always_parse_args;
	if(strncmp(cmd, arg_str, strlen(arg_str)) == 0) {
		arg_run = true;
		cmd = strchr(cmd, ' ') + 1;
	}

	if(wofi_mod_shift()) {
		wofi_write_cache(mode, cmd);
		wofi_term_run(cmd);
	}
	if(arg_run) {
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
		char* cache = utils_concat(2, "__args ", cmd);
		wofi_write_cache(mode, cache);
		free(cache);
		execvp(tmp, args);
	} else {
		wofi_write_cache(mode, cmd);
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

bool wofi_run_no_entry(void) {
	return true;
}
