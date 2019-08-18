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

static uint64_t width, height;
static int64_t x, y;
static struct zwlr_layer_shell_v1* shell;
static GtkWidget* window, *previous_selection = NULL;
static const gchar* filter;

static void nop() {}

static void add_interface(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
	(void) data;
	if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version);
	}
}

static void config_surface(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t _width, uint32_t _height) {
	(void) data;
	(void) _width;
	(void) _height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	zwlr_layer_surface_v1_set_size(surface, width, height);
	zwlr_layer_surface_v1_set_keyboard_interactivity(surface, true);
	if(x >= 0 && y >= 0) {
		zwlr_layer_surface_v1_set_margin(surface, y, 0, 0, x);
		zwlr_layer_surface_v1_set_anchor(surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	}
}

static void get_input(GtkSearchEntry* entry, gpointer data) {
	GtkListBox* box = data;
	filter = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_list_box_invalidate_filter(box);
}

static void do_run(GtkWidget* box) {
	char* path = strdup(getenv("PATH"));
	char* original_path = path;
	size_t colon_count = utils_split(path, ':');
	for(size_t count = 0; count < colon_count; ++count) {
		DIR* dir = opendir(path);
		if(dir == NULL) {
			continue;
		}
		struct dirent* entry;
		while((entry = readdir(dir)) != NULL) {
			if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			char* full_path = utils_concat(3, path, "/", entry->d_name);
			struct stat info;
			stat(full_path, &info);
			if(access(full_path, X_OK) == 0 && S_ISREG(info.st_mode)) {
				GtkWidget* label = gtk_label_new(entry->d_name);
				gtk_widget_set_name(label, "unselected");
				gtk_label_set_xalign(GTK_LABEL(label), 0);
				gtk_container_add(GTK_CONTAINER(box), label);
			}
			free(full_path);
		}
		path += strlen(path) + 1;
	}
	free(original_path);
}

static void do_dmenu(GtkWidget* box) {
	char* line;
	size_t size = 0;
	while(getline(&line, &size, stdin) != -1) {
		char* lf = strchr(line, '\n');
		if(lf != NULL) {
			*lf = 0;
		}
		GtkWidget* label = gtk_label_new(line);
		gtk_widget_set_name(label, "unselected");
		gtk_label_set_xalign(GTK_LABEL(label), 0);
		gtk_container_add(GTK_CONTAINER(box), label);
	}
	free(line);
}

static void do_drun(GtkWidget* box) {
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
			GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(full_path);
			free(full_path);
			if(!G_IS_DESKTOP_APP_INFO(info)) {
				continue;
			}
			const gchar* name = g_desktop_app_info_get_string(info, "Name");
			if(name == NULL) {
				continue;
			}
			GtkWidget* label = gtk_label_new(name);
			gtk_widget_set_name(label, "unselected");
			gtk_label_set_xalign(GTK_LABEL(label), 0);
			gtk_container_add(GTK_CONTAINER(box), label);
		}
		closedir(dir);
		cont:
		dirs += strlen(dirs) + 1;
		free(app_dir);
	}
	free(original_dirs);
}

static void execute_action(char* mode, const gchar* cmd) {
	if(strcmp(mode, "run") == 0) {
		execlp(cmd, cmd, NULL);
		fprintf(stderr, "%s cannot be executed\n", cmd);
		exit(errno);
	} else if(strcmp(mode, "dmenu") == 0) {
		printf("%s\n", cmd);
		exit(0);
	}
}

static void activate_item(GtkListBox* box, GtkListBoxRow* row, gpointer data) {
	(void) box;
	char* mode = data;
	GtkWidget* label = gtk_bin_get_child(GTK_BIN(row));
	execute_action(mode, gtk_label_get_text(GTK_LABEL(label)));
}

static void select_item(GtkListBox* box, GtkListBoxRow* row, gpointer data) {
	(void) box;
	(void) data;
	if(previous_selection != NULL) {
		gtk_widget_set_name(previous_selection, "unselected");
	}
	GtkWidget* label = gtk_bin_get_child(GTK_BIN(row));
	gtk_widget_set_name(label, "selected");
	previous_selection = label;
}

static void activate_search(GtkEntry* entry, gpointer data) {
	char* mode = data;
	execute_action(mode, gtk_entry_get_text(entry));
}

static gboolean do_filter(GtkListBoxRow* row, gpointer data) {
	(void) data;
	GtkWidget* label = gtk_bin_get_child(GTK_BIN(row));
	const gchar* text = gtk_label_get_text(GTK_LABEL(label));
	if(filter == NULL || strcmp(filter, "") == 0) {
		return TRUE;
	}
	if(strstr(text, filter) != NULL) {
		return TRUE;
	}
	return FALSE;
}

static gboolean escape(GtkWidget* widget, GdkEvent* event, gpointer data) {
	(void) widget;
	(void) event;
	(void) data;
	guint code;
	gdk_event_get_keyval(event, &code);
	if(code == GDK_KEY_Escape) {
		exit(0);
	}
	return FALSE;
}

void wofi_init(struct map* config) {
	width = strtol(config_get(config, "width", "1000"), NULL, 10);
	height = strtol(config_get(config, "height", "400"), NULL, 10);
	x = strtol(config_get(config, "x", "-1"), NULL, 10);
	y = strtol(config_get(config, "y", "-1"), NULL, 10);
	bool normal_window = strcmp(config_get(config, "normal_window", "false"), "true") == 0;
	char* mode = map_get(config, "mode");
	char* prompt = config_get(config, "prompt", mode);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(window);
	gtk_widget_set_name(window, "window");
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	gtk_window_resize(GTK_WINDOW(window), width, height);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	if(!normal_window) {
		GdkDisplay* disp = gdk_display_get_default();
		struct wl_display* wl = gdk_wayland_display_get_wl_display(disp);
		struct wl_registry* registry = wl_display_get_registry(wl);
		struct wl_registry_listener listener = {
			.global = add_interface,
			.global_remove = nop
		};
		wl_registry_add_listener(registry, &listener, NULL);
		wl_display_roundtrip(wl);
		GdkWindow* gdk_win = gtk_widget_get_window(window);
		gdk_wayland_window_set_use_custom_surface(gdk_win);
		struct wl_surface* wl_surface = gdk_wayland_window_get_wl_surface(gdk_win);
		struct zwlr_layer_surface_v1* surface = zwlr_layer_shell_v1_get_layer_surface(shell, wl_surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "wofi");
		struct zwlr_layer_surface_v1_listener* surface_listener = malloc(sizeof(struct zwlr_layer_surface_v1_listener));
		surface_listener->configure = config_surface;
		surface_listener->closed = nop;
		zwlr_layer_surface_v1_add_listener(surface, surface_listener, NULL);
		wl_surface_commit(wl_surface);
		wl_display_roundtrip(wl);
	}

	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(box, "outer-box");
	gtk_container_add(GTK_CONTAINER(window), box);
	GtkWidget* entry = gtk_search_entry_new();
	gtk_widget_set_name(entry, "input");
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), prompt);
	gtk_container_add(GTK_CONTAINER(box), entry);

	GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(scroll, "scroll");
	gtk_container_add(GTK_CONTAINER(box), scroll);
	gtk_widget_set_size_request(scroll, width, height);

	GtkWidget* inner_box = gtk_list_box_new();
	gtk_widget_set_name(inner_box, "inner-box");
	gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(inner_box), FALSE);
	gtk_container_add(GTK_CONTAINER(scroll), inner_box);

	gtk_list_box_set_filter_func(GTK_LIST_BOX(inner_box), do_filter, NULL, NULL);

	g_signal_connect(entry, "search-changed", G_CALLBACK(get_input), inner_box);
	g_signal_connect(inner_box, "row-activated", G_CALLBACK(activate_item), mode);
	g_signal_connect(inner_box, "row-selected", G_CALLBACK(select_item), NULL);
	g_signal_connect(entry, "activate", G_CALLBACK(activate_search), mode);
	g_signal_connect(window, "key-press-event", G_CALLBACK(escape), NULL);

	if(strcmp(mode, "run") == 0) {
		do_run(inner_box);
	} else if(strcmp(mode, "dmenu") == 0) {
		do_dmenu(inner_box);
	} else if(strcmp(mode, "drun") == 0) {
		do_drun(inner_box);
	} else {
		fprintf(stderr, "I would love to show %s but Idk what it is\n", mode);
		exit(1);
	}
	gtk_widget_grab_focus(entry);
	gtk_widget_show_all(window);
}
