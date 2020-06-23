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

#include <stdio.h>
#include <libgen.h>

#include <sys/stat.h>

#include <map.h>
#include <utils.h>
#include <config.h>
#include <wofi_api.h>

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

static const char* arg_names[] = {"print_command", "display_generic"};

static struct mode* mode;

struct desktop_entry {
	char* full_path;
	struct wl_list link;
};

static struct map* entries;
static struct wl_list desktop_entries;

static bool print_command;
static bool display_generic;

static char* get_text(char* file, char* action) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(file);
	if(info == NULL || g_desktop_app_info_get_is_hidden(info) || g_desktop_app_info_get_nodisplay(info)) {
		return NULL;
	}
	const char* name;
	char* generic_name = strdup("");
	if(action == NULL) {
		name = g_app_info_get_display_name(G_APP_INFO(info));
		if(display_generic) {
			const char* gname = g_desktop_app_info_get_generic_name(info);
			if(gname != NULL) {
				free(generic_name);
				generic_name = utils_concat(3, " (", gname, ")");
			}
		}
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
			char* ret = utils_concat(5, "img:", path, ":text:", name, generic_name);
			free(generic_name);
			return ret;
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
			char* ret = utils_concat(5, "img:", icon_path, ":text:", name, generic_name);
			free(generic_name);
			return ret;
		}
	} else {
		char* ret = utils_concat(2, name, generic_name);
		free(generic_name);
		return ret;
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

static struct widget* create_widget(char* full_path) {
	char* id = get_id(full_path);

	if(map_contains(entries, id)) {
		free(id);
		free(full_path);
		return NULL;
	}

	size_t action_count;
	char** text = get_action_text(full_path, &action_count);
	map_put(entries, id, "true");
	if(text == NULL) {
		wofi_remove_cache(mode, full_path);
		free(id);
		free(full_path);
		return NULL;
	}

	char** actions = get_action_actions(full_path, &action_count);

	char* search_text = get_search_text(full_path);

	struct widget* ret = wofi_create_widget(mode, text, search_text, actions, action_count);

	for(size_t count = 0; count < action_count; ++count) {
		free(actions[count]);
		free(text[count]);
	}

	free(id);
	free(text);
	free(actions);
	free(full_path);
	free(search_text);

	return ret;
}

static void insert_dir(char* app_dir) {
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
			insert_dir(full_path);
			free(id);
			free(full_path);
			continue;
		}
		if(map_contains(entries, id)) {
			free(id);
			free(full_path);
			continue;
		}

		struct desktop_entry* entry = malloc(sizeof(struct desktop_entry));
		entry->full_path = full_path;
		wl_list_insert(&desktop_entries, &entry->link);

		free(id);
	}
	closedir(dir);
}

static char* get_data_dirs(void) {
	char* data_dirs = getenv("XDG_DATA_DIRS");
	if(data_dirs == NULL) {
		data_dirs = "/usr/local/share:/usr/share";
	}
	return strdup(data_dirs);
}

static char* get_data_home(void) {
	char* data_home = getenv("XDG_DATA_HOME");
	if(data_home == NULL) {
		data_home = utils_concat(2, getenv("HOME"), "/.local/share");
	} else {
		data_home = strdup(data_home);
	}
	return data_home;
}

static bool starts_with_data_dirs(char* path) {
	char* data_dirs = get_data_dirs();
	char* save_ptr;
	char* str = strtok_r(data_dirs, ":", &save_ptr);
	do {
		char* tmpstr = utils_concat(2, str, "/applications");
		char* tmp = strdup(path);
		char* dir = dirname(tmp);
		if(strcmp(dir, tmpstr) == 0) {
			free(tmp);
			free(data_dirs);
			free(tmpstr);
			return true;
		}
		free(tmp);
		free(tmpstr);
	} while((str = strtok_r(NULL, ":", &save_ptr)) != NULL);

	free(data_dirs);
	return false;
}

static bool should_invalidate_cache(char* path) {
	if(starts_with_data_dirs(path)) {
		char* data_home = get_data_home();
		char* tmp = strdup(path);
		char* file = basename(tmp);
		char* full_path = utils_concat(3, data_home, "/applications/", file);
		free(data_home);
		if(access(full_path, F_OK) == 0) {
			free(full_path);
			free(tmp);
			return true;
		}
		free(full_path);
		free(tmp);
	}
	return false;
}

void wofi_drun_init(struct mode* this, struct map* config) {
	mode = this;

	print_command = strcmp(config_get(config, "print_command", "false"), "true") == 0;
	display_generic = strcmp(config_get(config, "display_generic", "false"), "true") == 0;

	entries = map_init();
	struct wl_list* cache = wofi_read_cache(mode);

	wl_list_init(&desktop_entries);

	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, cache, link) {
		if(should_invalidate_cache(node->line)) {
			wofi_remove_cache(mode, node->line);
			free(node->line);
			goto cache_cont;
		}

		struct desktop_entry* entry = malloc(sizeof(struct desktop_entry));
		entry->full_path = node->line;
		wl_list_insert(&desktop_entries, &entry->link);

		cache_cont:
		wl_list_remove(&node->link);
		free(node);
	}

	free(cache);

	char* data_home = get_data_home();
	char* data_dirs = get_data_dirs();
	char* dirs = utils_concat(3, data_home, ":", data_dirs);
	free(data_home);
	free(data_dirs);

	char* save_ptr;
	char* str = strtok_r(dirs, ":", &save_ptr);
	do {
		char* app_dir = utils_concat(2, str, "/applications");
		insert_dir(app_dir);
		free(app_dir);
	} while((str = strtok_r(NULL, ":", &save_ptr)) != NULL);
	free(dirs);
}

struct widget* wofi_drun_get_widget(void) {
	struct desktop_entry* node, *tmp;
	wl_list_for_each_reverse_safe(node, tmp, &desktop_entries, link) {
		struct widget* widget = create_widget(node->full_path);
		wl_list_remove(&node->link);
		free(node);
		if(widget == NULL) {
			continue;
		}
		return widget;
	}
	return NULL;
}

static void launch_done(GObject* obj, GAsyncResult* result, gpointer data) {
	GError* err = NULL;
	if(g_app_info_launch_uris_finish(G_APP_INFO(obj), result, &err)) {
		exit(0);
	} else if(err != NULL) {
		char* cmd = data;
		fprintf(stderr, "%s cannot be executed: %s\n", cmd, err->message);
		g_error_free(err);
	} else {
		char* cmd = data;
		fprintf(stderr, "%s cannot be executed\n", cmd);
	}
	exit(1);
}

static void set_dri_prime(GDesktopAppInfo* info) {
	bool dri_prime = g_desktop_app_info_get_boolean(info, "PrefersNonDefaultGPU");
	if(dri_prime) {
		setenv("DRI_PRIME", "1", true);
	}
}

void wofi_drun_exec(const gchar* cmd) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(cmd);
	if(G_IS_DESKTOP_APP_INFO(info)) {
		wofi_write_cache(mode, cmd);
		if(print_command) {
			printf("%s\n", g_app_info_get_commandline(G_APP_INFO(info)));
			exit(0);
		} else {
			set_dri_prime(info);
			g_app_info_launch_uris_async(G_APP_INFO(info), NULL, NULL, NULL, launch_done, (gchar*) cmd);
		}
	} else if(strrchr(cmd, ' ') != NULL) {
		char* space = strrchr(cmd, ' ');
		*space = 0;
		wofi_write_cache(mode, cmd);
		info = g_desktop_app_info_new_from_filename(cmd);
		char* action = space + 1;
		if(print_command) {
			printf("%s\n", g_app_info_get_commandline(G_APP_INFO(info)));
			fprintf(stderr, "Printing the command line for an action is not supported\n");
		} else {
			set_dri_prime(info);
			g_desktop_app_info_launch_action(info, action, NULL);
			utils_sleep_millis(500);
		}
		exit(0);
	} else {
		fprintf(stderr, "%s cannot be executed\n", cmd);
		exit(1);
	}
}

const char** wofi_drun_get_arg_names(void) {
	return arg_names;
}

size_t wofi_drun_get_arg_count(void) {
	return sizeof(arg_names) / sizeof(char*);
}
