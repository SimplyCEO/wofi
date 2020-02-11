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

static const char* arg_names[] = {"parse_action", "separator", "print_line_num"};

static bool parse_action;
static char* separator;
static bool print_line_num;
static struct mode* mode;

struct node {
	struct widget* widget;
	struct wl_list link;
};

static struct wl_list widgets;

void wofi_dmenu_init(struct mode* this, struct map* config) {
	mode = this;
	parse_action = strcmp(config_get(config, "parse_action", "false"), "true") == 0;
	separator = config_get(config, "separator", "\n");
	print_line_num = strcmp(config_get(config, "print_line_num", "false"), "true") == 0;

	if(strcmp(separator, "\\n") == 0) {
		separator = "\n";
	} else if(strcmp(separator, "\\0") == 0) {
		separator = "\0";
	}

	wl_list_init(&widgets);

	struct map* cached = map_init();

	struct wl_list entries;
	wl_list_init(&entries);

	struct map* entry_map = map_init();

	char* line = NULL;
	size_t size = 0;
	while(getdelim(&line, &size, separator[0], stdin) != -1) {
		char* delim = strchr(line, separator[0]);
		if(delim != NULL) {
			*delim = 0;
		}
		struct cache_line* node = malloc(sizeof(struct cache_line));
		node->line = strdup(line);
		wl_list_insert(&entries, &node->link);
		map_put(entry_map, line, "true");
	}
	free(line);

	if(!print_line_num) {
		struct wl_list* cache = wofi_read_cache(mode);

		struct cache_line* node, *tmp;
		wl_list_for_each_safe(node, tmp, cache, link) {
			if(map_contains(entry_map, node->line)) {
				map_put(cached, node->line, "true");
				struct node* widget = malloc(sizeof(struct node));
				widget->widget = wofi_create_widget(mode, &node->line, node->line, &node->line, 1);
				wl_list_insert(&widgets, &widget->link);
			} else {
				wofi_remove_cache(mode, node->line);
			}
			free(node->line);
			wl_list_remove(&node->link);
			free(node);
		}
		free(cache);
	}

	map_free(entry_map);

	uint16_t line_num = 0;

	struct cache_line* node, *tmp;
	wl_list_for_each_reverse_safe(node, tmp, &entries, link) {
		if(!map_contains(cached, node->line)) {

			char* action;
			if(print_line_num) {
				action = malloc(6);
				snprintf(action, 6, "%u", line_num++);
			} else {
				action = strdup(node->line);
			}

			struct node* widget = malloc(sizeof(struct node));
			widget->widget = wofi_create_widget(mode, &node->line, node->line, &action, 1);
			wl_list_insert(&widgets, &widget->link);
			free(action);
		}
		free(node->line);
		wl_list_remove(&node->link);
		free(node);
	}
	map_free(cached);
}

struct widget* wofi_dmenu_get_widget(void) {
	struct node* node, *tmp;
	wl_list_for_each_reverse_safe(node, tmp, &widgets, link) {
		struct widget* widget = node->widget;
		wl_list_remove(&node->link);
		free(node);
		return widget;
	}
	return NULL;
}

void wofi_dmenu_exec(const gchar* cmd) {
	char* action = strdup(cmd);
	if(parse_action) {
		if(wofi_allow_images()) {
			free(action);
			action = wofi_parse_image_escapes(cmd);
		}
		if(wofi_allow_markup()) {
			char* out;
			pango_parse_markup(action, -1, 0, NULL, &out, NULL, NULL);
			free(action);
			action = out;
		}
	}
	if(!print_line_num) {
		wofi_write_cache(mode, cmd);
	}
	printf("%s\n", action);
	free(action);
	exit(0);
}

const char** wofi_dmenu_get_arg_names(void) {
	return arg_names;
}

size_t wofi_dmenu_get_arg_count(void) {
	return sizeof(arg_names) / sizeof(char*);
}

bool wofi_dmenu_no_entry(void) {
	return true;
}
