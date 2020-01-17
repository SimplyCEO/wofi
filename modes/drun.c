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

#define MODE "drun"

struct node {
	struct widget* widget;
	struct wl_list link;
};

static struct wl_list widgets;

static char* get_text(char* file, char* action) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(file);
	if(info == NULL || g_desktop_app_info_get_is_hidden(info) || g_desktop_app_info_get_nodisplay(info)) {
		return NULL;
	}
	const char* name;
	if(action == NULL) {
		name = g_app_info_get_display_name(G_APP_INFO(info));
	} else {
		name = g_desktop_app_info_get_action_name(info, action);
	}
	if(name == NULL) {
		return NULL;
	}
	if(wofi_allow_images()) {
		GIcon* icon = g_app_info_get_icon(G_APP_INFO(info));
		if(G_IS_FILE_ICON(icon)) {
			GFile* file = g_file_icon_get_file(G_FILE_ICON(icon));
			char* path = g_file_get_path(file);
			return utils_concat(4, "img:", path, ":text:", name);
		} else {
			GtkIconTheme* theme = gtk_icon_theme_get_default();
			GtkIconInfo* info = NULL;
			if(icon != NULL) {
				const gchar* const* icon_names = g_themed_icon_get_names(G_THEMED_ICON(icon));
				info = gtk_icon_theme_choose_icon(theme, (const gchar**) icon_names, wofi_get_image_size(), 0);
			}
			if(info == NULL) {
				info = gtk_icon_theme_lookup_icon(theme, "application-x-executable", wofi_get_image_size(), 0);
			}
			const gchar* icon_path = gtk_icon_info_get_filename(info);
			return utils_concat(4, "img:", icon_path, ":text:", name);
		}
	} else {
		return strdup(name);
	}
}

static char* get_search_text(char* file) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(file);
	const char* name = g_app_info_get_display_name(G_APP_INFO(info));
	const char* exec = g_app_info_get_executable(G_APP_INFO(info));
	const char* description = g_app_info_get_description(G_APP_INFO(info));
	const char* categories = g_desktop_app_info_get_categories(info);
	const char* const* keywords = g_desktop_app_info_get_keywords(info);
	const char* generic_name = g_desktop_app_info_get_generic_name(info);

	char* keywords_str = strdup("");

	if(keywords != NULL) {
		for(size_t count = 0; keywords[count] != NULL; ++count) {
			char* tmp = utils_concat(2, keywords_str, keywords[count]);
			free(keywords_str);
			keywords_str = tmp;
		}
	}

	char* ret = utils_concat(7, name, file,
			exec == NULL ? "" : exec,
			description == NULL ? "" : description,
			categories == NULL ? "" : categories,
			keywords_str,
			generic_name == NULL ? "" : generic_name);
	free(keywords_str);
	return ret;
}

static const gchar* const* get_actions(char* file, size_t* action_count) {
	*action_count = 0;
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(file);
	if(info == NULL) {
		return NULL;
	}
	const gchar* const* actions = g_desktop_app_info_list_actions(info);
	if(actions[0] == NULL) {
		return NULL;
	}

	for(; actions[*action_count] != NULL; ++*action_count);

	return actions;
}

static char** get_action_text(char* file, size_t* text_count) {
	*text_count = 0;

	char* tmp = get_text(file, NULL);
	if(tmp == NULL) {
		return NULL;
	}

	const gchar* const* action_names = get_actions(file, text_count);

	++*text_count;
	char** text = malloc(*text_count * sizeof(char*));
	text[0] = tmp;

	for(size_t count = 1; count < *text_count; ++count) {
		text[count] = get_text(file, (gchar*) action_names[count - 1]);
	}
	return text;
}

static char** get_action_actions(char* file, size_t* action_count) {
	*action_count = 0;

	char* tmp = strdup(file);
	if(tmp == NULL) {
		return NULL;
	}

	const gchar* const* action_names = get_actions(file, action_count);

	++*action_count;
	char** actions = malloc(*action_count * sizeof(char*));
	actions[0] = tmp;

	for(size_t count = 1; count < *action_count; ++count) {
		actions[count] = utils_concat(3, file, " ", (gchar*) action_names[count - 1]);
	}
	return actions;
}

static char* get_id(char* path) {
	char* applications = strstr(path, "applications/");
	char* name = strchr(applications, '/') + 1;
	char* id = strdup(name);

	char* slash;
	while((slash = strchr(id, '/')) != NULL) {
		*slash = '-';
	}
	return id;
}

static void insert_dir(char* app_dir, struct map* cached, struct map* entries) {
	DIR* dir = opendir(app_dir);
	if(dir == NULL) {
		return;
	}
	struct dirent* entry;
	while((entry = readdir(dir)) != NULL) {
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		char* full_path = utils_concat(3, app_dir, "/", entry->d_name);
		char* id = get_id(full_path);

		struct stat info;
		stat(full_path, &info);
		if(S_ISDIR(info.st_mode)) {
			insert_dir(full_path, cached, entries);
			free(id);
			free(full_path);
			continue;
		}
		if(map_contains(cached, id)) {
			free(id);
			free(full_path);
			continue;
		}
		if(map_contains(entries, id)) {
			free(id);
			free(full_path);
			continue;
		}
		size_t action_count;
		char** text = get_action_text(full_path, &action_count);
		map_put(entries, id, "true");
		if(text == NULL) {
			free(id);
			free(full_path);
			continue;
		}

		char** actions = get_action_actions(full_path, &action_count);

		char* search_text = get_search_text(full_path);

		struct node* node = malloc(sizeof(struct node));
		node->widget = wofi_create_widget(MODE, text, search_text, actions, action_count);
		wl_list_insert(&widgets, &node->link);

		for(size_t count = 0; count < action_count; ++count) {
			free(actions[count]);
			free(text[count]);
		}

		free(id);
		free(text);
		free(actions);
		free(search_text);
		free(full_path);
	}
	closedir(dir);
}

void wofi_drun_init(void) {
	struct map* cached = map_init();
	struct map* entries = map_init();
	struct wl_list* cache = wofi_read_cache(MODE);

	wl_list_init(&widgets);

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		size_t action_count;
		char** text = get_action_text(node->line, &action_count);
		if(text == NULL) {
			wofi_remove_cache(MODE, node->line);
			goto cache_cont;
		}

		char** actions = get_action_actions(node->line, &action_count);

		char* search_text = get_search_text(node->line);
		struct node* widget = malloc(sizeof(struct node));
		widget->widget = wofi_create_widget(MODE, text, search_text, actions, action_count);
		wl_list_insert(&widgets, &widget->link);

		char* id = get_id(node->line);

		map_put(cached, id, "true");

		free(id);

		free(search_text);

		for(size_t count = 0; count < action_count; ++count) {
			free(text[count]);
			free(actions[count]);
		}
		free(text);
		free(actions);

		cache_cont:
		free(node->line);
		wl_list_remove(&node->link);
		free(node);
	}

	free(cache);

	char* data_home = getenv("XDG_DATA_HOME");
	if(data_home == NULL) {
		data_home = utils_concat(2, getenv("HOME"), "/.local/share");
	} else {
		data_home = strdup(data_home);
	}
	char* data_dirs = getenv("XDG_DATA_DIRS");
	if(data_dirs == NULL) {
		data_dirs = "/usr/local/share:/usr/share";
	}
	char* dirs = utils_concat(3, data_home, ":", data_dirs);
	free(data_home);

	char* save_ptr;
	char* str = strtok_r(dirs, ":", &save_ptr);
	do {
		char* app_dir = utils_concat(2, str, "/applications");
		insert_dir(app_dir, cached, entries);
		free(app_dir);
	} while((str = strtok_r(NULL, ":", &save_ptr)) != NULL);
	free(dirs);
	map_free(cached);
	map_free(entries);
}

struct widget* wofi_drun_get_widget() {
	struct node* node, *tmp;
	wl_list_for_each_reverse_safe(node, tmp, &widgets, link) {
		struct widget* widget = node->widget;
		wl_list_remove(&node->link);
		free(node);
		return widget;
	}
	return NULL;
}

static void launch_done(GObject* obj, GAsyncResult* result, gpointer data) {
	if(g_app_info_launch_uris_finish(G_APP_INFO(obj), result, NULL)) {
		exit(0);
	} else {
		char* cmd = data;
		fprintf(stderr, "%s cannot be executed\n", cmd);
		exit(1);
	}
}

void wofi_drun_exec(const gchar* cmd) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(cmd);
	if(G_IS_DESKTOP_APP_INFO(info)) {
		wofi_write_cache(MODE, cmd);
		g_app_info_launch_uris_async(G_APP_INFO(info), NULL, NULL, NULL, launch_done, (gchar*) cmd);
	} else if(strrchr(cmd, ' ') != NULL) {
		char* space = strrchr(cmd, ' ');
		*space = 0;
		wofi_write_cache(MODE, cmd);
		info = g_desktop_app_info_new_from_filename(cmd);
		char* action = space + 1;
		g_desktop_app_info_launch_action(info, action, NULL);
		utils_sleep_millis(500);
		exit(0);
	} else {
		fprintf(stderr, "%s cannot be executed\n", cmd);
		exit(1);
	}
}
