/*
 *  Copyright (C) 2019-2023 Scoopta
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
#include <utils_g.h>
#include <widget_builder_api.h>

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

static const char* arg_names[] = {"print_command", "display_generic", "disable_prime", "print_desktop_file"};

static struct mode* mode;

struct desktop_entry {
	char* full_path;
	struct wl_list link;
};

static struct map* entries;
static struct wl_list desktop_entries;

static bool print_command;
static bool display_generic;
static bool disable_prime;
static bool print_desktop_file;

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

static bool populate_widget(char* file, char* action, struct widget_builder* builder) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(file);
	if(info == NULL || !g_app_info_should_show(G_APP_INFO(info)) ||
			g_desktop_app_info_get_is_hidden(info)) {
		return false;
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
		free(generic_name);
		return false;
	}



	if(wofi_allow_images()) {
		GIcon* icon = g_app_info_get_icon(G_APP_INFO(info));
		GdkPixbuf* pixbuf;
		if(G_IS_FILE_ICON(icon)) {
			GFile* file = g_file_icon_get_file(G_FILE_ICON(icon));
			char* path = g_file_get_path(file);
			pixbuf = gdk_pixbuf_new_from_file(path, NULL);
		} else {
			GtkIconTheme* theme = gtk_icon_theme_get_default();
			GtkIconInfo* info = NULL;
			if(icon != NULL) {
				const gchar* const* icon_names = g_themed_icon_get_names(G_THEMED_ICON(icon));
				info = gtk_icon_theme_choose_icon_for_scale(theme, (const gchar**) icon_names, wofi_get_image_size(), wofi_get_window_scale(), 0);
			}
			if(info == NULL) {
				info = gtk_icon_theme_lookup_icon_for_scale(theme, "application-x-executable", wofi_get_image_size(), wofi_get_window_scale(), 0);
			}
			pixbuf = gtk_icon_info_load_icon(info, NULL);
		}

		if(pixbuf == NULL) {
			goto img_fail;
		}

		pixbuf = utils_g_resize_pixbuf(pixbuf, wofi_get_image_size() * wofi_get_window_scale(), GDK_INTERP_BILINEAR);

		wofi_widget_builder_insert_image(builder, pixbuf, "icon", NULL);
		g_object_unref(pixbuf);
	}

	img_fail:
	wofi_widget_builder_insert_text(builder, name, "name", NULL);
	wofi_widget_builder_insert_text(builder, generic_name, "generic-name", NULL);
	free(generic_name);

	if(action == NULL) {
		wofi_widget_builder_set_action(builder, file);
	} else {
		char* action_txt = utils_concat(3, file, " ", action);
		wofi_widget_builder_set_action(builder, action_txt);
		free(action_txt);
	}


	char* search_txt = get_search_text(file);
	wofi_widget_builder_set_search_text(builder, search_txt);
	free(search_txt);

	return true;
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

static struct widget_builder* populate_actions(char* file, size_t* text_count) {
	const gchar* const* action_names = get_actions(file, text_count);

	++*text_count;


	struct widget_builder* builder = wofi_widget_builder_init(mode, *text_count);
	if(!populate_widget(file, NULL, builder)) {
		wofi_widget_builder_free(builder);
		return NULL;
	}

	for(size_t count = 1; count < *text_count; ++count) {
		populate_widget(file, (gchar*) action_names[count - 1], wofi_widget_builder_get_idx(builder, count));
	}
	return builder;
}

static char* get_id(char* path) {
	char* applications = strstr(path, "applications/");
	if(applications == NULL) {
		return NULL;
	}
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
	if(id == NULL) {
		free(full_path);
		return NULL;
	}

	if(map_contains(entries, id)) {
		free(id);
		free(full_path);
		return NULL;
	}

	map_put(entries, id, "true");

	size_t action_count;

	struct widget_builder* builder = populate_actions(full_path, &action_count);
	if(builder == NULL) {
		wofi_remove_cache(mode, full_path);
		free(id);
		free(full_path);
		return NULL;
	}

	struct widget* ret = wofi_widget_builder_get_widget(builder);

	free(id);
	free(full_path);

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
		if(id == NULL) {
			free(full_path);
			continue;
		}

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
	disable_prime = strcmp(config_get(config, "disable_prime", "false"), "true") == 0;
	print_desktop_file = strcmp(config_get(config, "print_desktop_file", "false"), "true") == 0;

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
	if(dri_prime && !disable_prime) {
		setenv("DRI_PRIME", "1", true);
	}
}

static bool uses_dbus(GDesktopAppInfo* info) {
	return g_desktop_app_info_get_boolean(info, "DBusActivatable");
}

static char* get_cmd(GAppInfo* info) {
	const char* cmd = g_app_info_get_commandline(info);
	size_t cmd_size = strlen(cmd);
	char* new_cmd = calloc(1, cmd_size + 1);
	size_t new_cmd_count = 0;
	for(size_t count = 0; count < cmd_size; ++count) {
		if(cmd[count] == '%') {
			if(cmd[++count] == '%') {
				new_cmd[new_cmd_count++] = cmd[count];
			}
		} else {
			new_cmd[new_cmd_count++] = cmd[count];
		}
	}
	if(new_cmd[--new_cmd_count] == ' ') {
		new_cmd[new_cmd_count] = 0;
	}
	return new_cmd;
}

void wofi_drun_exec(const gchar* cmd) {
	GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(cmd);
	if(G_IS_DESKTOP_APP_INFO(info)) {
		wofi_write_cache(mode, cmd);
		if(print_command) {
			char* cmd = get_cmd(G_APP_INFO(info));
			printf("%s\n", cmd);
			free(cmd);
			exit(0);
		} else if(print_desktop_file) {
			printf("%s\n", cmd);
			exit(0);
		} else {
			set_dri_prime(info);
			if(uses_dbus(info)) {
				g_app_info_launch_uris_async(G_APP_INFO(info), NULL, NULL, NULL, launch_done, (gchar*) cmd);
			} else {
				g_app_info_launch_uris(G_APP_INFO(info), NULL, NULL, NULL);
				exit(0);
			}
		}
	} else if(strrchr(cmd, ' ') != NULL) {
		char* space = strrchr(cmd, ' ');
		*space = 0;
		wofi_write_cache(mode, cmd);
		info = g_desktop_app_info_new_from_filename(cmd);
		char* action = space + 1;
		if(print_command) {
			char* cmd = get_cmd(G_APP_INFO(info));
			printf("%s\n", cmd);
			free(cmd);
			fprintf(stderr, "Printing the command line for an action is not supported\n");
		} else if(print_desktop_file) {
			printf("%s %s\n", cmd, action);
			exit(0);
		} else {
			set_dri_prime(info);
			g_desktop_app_info_launch_action(info, action, NULL);
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
