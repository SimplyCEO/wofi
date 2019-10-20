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

static char* get_text(char* file) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(file);
	if(info == NULL || g_desktop_app_info_get_is_hidden(info) || g_desktop_app_info_get_nodisplay(info)) {
		return NULL;
	}
	const char* name = g_app_info_get_display_name(G_APP_INFO(info));
	if(name == NULL) {
		return NULL;
	}
	if(wofi_allow_images()) {
		GIcon* icon = g_app_info_get_icon(G_APP_INFO(info));
		if(G_IS_THEMED_ICON(icon)) {
			GtkIconTheme* theme = gtk_icon_theme_get_default();
			const gchar* const* icon_names = g_themed_icon_get_names(G_THEMED_ICON(icon));
			GtkIconInfo* info = gtk_icon_theme_choose_icon(theme, (const gchar**) icon_names, wofi_get_image_size(), 0);
			if(info == NULL) {
				return utils_concat(2, "text:", name);
			} else {
				const gchar* icon_path = gtk_icon_info_get_filename(info);
				return utils_concat(4, "img:", icon_path, ":text:", name);
			}
		} else {
			return utils_concat(2, "text:", name);
		}
	} else {
		return strdup(name);
	}
}

void drun_init() {
	struct map* cached = map_init();
	struct map* entries = map_init();
	struct wl_list* cache = wofi_read_cache("drun");

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		char* text = get_text(node->line);
		if(text == NULL) {
			goto cache_cont;
		}
		wofi_insert_widget(text, node->line);
		map_put(cached, node->line, "true");
		free(text);

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
	char* original_dirs = dirs;
	free(data_home);

	size_t colon_count = utils_split(dirs, ':');
	for(size_t count = 0; count < colon_count; ++count) {
		char* app_dir = utils_concat(2, dirs, "/applications");
		DIR* dir = opendir(app_dir);
		if(dir == NULL) {
			goto cont;
		}
		struct dirent* entry;
		while((entry = readdir(dir)) != NULL) {
			if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			char* full_path = utils_concat(3, app_dir, "/", entry->d_name);
			if(map_contains(cached, full_path)) {
				free(full_path);
				continue;
			}
			char* text = get_text(full_path);
			if(text == NULL) {
				free(full_path);
				continue;
			}
			if(map_contains(entries, entry->d_name)) {
				continue;
			}
			map_put(entries, entry->d_name, "true");
			wofi_insert_widget(text, full_path);
			free(text);
			free(full_path);
		}
		closedir(dir);
		cont:
		dirs += strlen(dirs) + 1;
		free(app_dir);
	}
	free(original_dirs);
	map_free(cached);
	map_free(entries);
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

void drun_exec(const gchar* cmd) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(cmd);
	if(G_IS_DESKTOP_APP_INFO(info)) {
		g_app_info_launch_uris_async(G_APP_INFO(info), NULL, NULL, NULL, launch_done, (gchar*) cmd);
	} else {
		fprintf(stderr, "%s cannot be executed\n", cmd);
		exit(1);
	}
}
