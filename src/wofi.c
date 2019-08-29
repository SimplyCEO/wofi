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
static GtkWidget* window, *outer_box, *scroll, *entry, *inner_box, *previous_selection = NULL;
static const gchar* filter;
static char* mode;
static time_t filter_time;
static int64_t filter_rate;
static bool allow_images, allow_markup;

struct node {
	char* text, *action;
	GtkContainer* container;
};

struct cache_line {
	char* line;
	struct wl_list link;
};

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
	(void) data;
	if(utils_get_time_millis() - filter_time > filter_rate) {
		filter = gtk_entry_get_text(GTK_ENTRY(entry));
		filter_time = utils_get_time_millis();
		gtk_list_box_invalidate_filter(GTK_LIST_BOX(inner_box));
	}
}

static void get_search(GtkSearchEntry* entry, gpointer data) {
	(void) data;
	filter = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_list_box_invalidate_filter(GTK_LIST_BOX(inner_box));
}

static GtkWidget* create_label(char* text, char* action) {
	GtkWidget* box = wofi_property_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(box, "unselected");
	wofi_property_box_add_property(WOFI_PROPERTY_BOX(box), "action", action);
	if(allow_images) {
		char* tmp = strdup(text);
		char* original = tmp;
		char* mode = NULL;
		char* filter = NULL;

		size_t colon_count = utils_split(tmp, ':');
		for(size_t count = 0; count < colon_count; ++count) {
			if(mode == NULL) {
				mode = tmp;
			} else {
				if(strcmp(mode, "img") == 0) {
					GdkPixbuf* buf = gdk_pixbuf_new_from_file(tmp, NULL);
					int width = gdk_pixbuf_get_width(buf);
					int height = gdk_pixbuf_get_height(buf);
					if(height > width) {
						float percent = 32.f / height;
						GdkPixbuf* tmp = gdk_pixbuf_scale_simple(buf, width * percent, 32, GDK_INTERP_BILINEAR);
						g_object_unref(buf);
						buf = tmp;
					} else {
						float percent = 32.f / width;
						GdkPixbuf* tmp = gdk_pixbuf_scale_simple(buf, 32, height * percent, GDK_INTERP_BILINEAR);
						g_object_unref(buf);
						buf = tmp;
					}
					GtkWidget* img = gtk_image_new_from_pixbuf(buf);
					gtk_container_add(GTK_CONTAINER(box), img);
				} else if(strcmp(mode, "text") == 0) {
					if(filter == NULL) {
						filter = strdup(tmp);
					} else {
						char* tmp_filter = utils_concat(2, filter, tmp);
						free(filter);
						filter = tmp_filter;
					}
					GtkWidget* label = gtk_label_new(tmp);
					gtk_label_set_use_markup(GTK_LABEL(label), allow_markup);
					gtk_label_set_xalign(GTK_LABEL(label), 0);
					gtk_container_add(GTK_CONTAINER(box), label);
				}
				mode = NULL;
			}
			tmp += strlen(tmp) + 1;
		}
		wofi_property_box_add_property(WOFI_PROPERTY_BOX(box), "filter", filter);
		free(filter);
		free(original);
	} else {
		wofi_property_box_add_property(WOFI_PROPERTY_BOX(box), "filter", text);
		GtkWidget* label = gtk_label_new(text);
		gtk_label_set_use_markup(GTK_LABEL(label), allow_markup);
		gtk_label_set_xalign(GTK_LABEL(label), 0);
		gtk_container_add(GTK_CONTAINER(box), label);
	}

	return box;
}

static gboolean insert_widget(gpointer data) {
	struct node* node = data;
	GtkWidget* box = create_label(node->text, node->action);
	gtk_container_add(node->container, box);
	gtk_widget_show_all(box);
	free(node->text);
	free(node->action);
	free(node);
	return FALSE;
}

static char* get_cache_path(char* mode) {
	char* cache_path = getenv("XDG_CACHE_HOME");
	if(cache_path == NULL) {
		cache_path = utils_concat(3, getenv("HOME"), "/.cache/wofi-", mode);
	} else {
		cache_path = utils_concat(3, cache_path, "/wofi-", mode);
	}
	return cache_path;
}

static struct wl_list* read_cache(char* mode) {
	char* cache_path = get_cache_path(mode);
	struct wl_list* cache = malloc(sizeof(struct wl_list));
	wl_list_init(cache);
	struct wl_list lines;
	wl_list_init(&lines);
	if(access(cache_path, R_OK) == 0) {
		FILE* file = fopen(cache_path, "r");
		char* line = NULL;
		size_t size = 0;
		while(getline(&line, &size, file) != -1) {
			struct cache_line* node = malloc(sizeof(struct cache_line));
			char* lf = strchr(line, '\n');
			if(lf != NULL) {
				*lf = 0;
			}
			node->line = strdup(line);
			wl_list_insert(&lines, &node->link);
		}
		free(line);
		fclose(file);
	}
	while(wl_list_length(&lines) > 0) {
		uint64_t largest = 0;
		struct cache_line* node, *largest_node = NULL;
		wl_list_for_each(node, &lines, link) {
			uint64_t num = strtol(node->line, NULL, 10);
			if(num > largest) {
				largest = num;
				largest_node = node;
			}
		}
		wl_list_remove(&largest_node->link);
		wl_list_insert(cache, &largest_node->link);
	}
	free(cache_path);
	return cache;
}

static void* do_run(void* data) {
	(void) data;
	struct map* cached = map_init();
	struct wl_list* cache = read_cache("run");
	struct cache_line* node, *tmp;
	wl_list_for_each_reverse_safe(node, tmp, cache, link) {
		struct node* label = malloc(sizeof(struct node));
		char* text = strrchr(node->line, '/');
		char* action = strchr(node->line, ' ') + 1;
		if(text == NULL) {
			text = action;
		} else {
			++text;
		}
		map_put(cached, action, "true");
		label->text = strdup(text);
		label->action = strdup(action);
		label->container = GTK_CONTAINER(inner_box);
		g_idle_add(insert_widget, label);
		utils_sleep_millis(1);
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
				struct node* node = malloc(sizeof(struct node));
				node->text = strdup(entry->d_name);
				node->action = strdup(full_path);
				node->container = GTK_CONTAINER(inner_box);
				g_idle_add(insert_widget, node);
				utils_sleep_millis(1);
			}
			free(full_path);
		}
		closedir(dir);
		cont:
		path += strlen(path) + 1;
	}
	free(original_path);
	map_free(cached);
	return NULL;
}

static void* do_dmenu(void* data) {
	(void) data;
	char* line;
	size_t size = 0;
	while(getline(&line, &size, stdin) != -1) {
		char* lf = strchr(line, '\n');
		if(lf != NULL) {
			*lf = 0;
		}
		struct node* node = malloc(sizeof(struct node));
		node->text = strdup(line);
		node->action = strdup(line);
		node->container = GTK_CONTAINER(inner_box);
		g_idle_add(insert_widget, node);
		utils_sleep_millis(1);
	}
	free(line);
	return NULL;
}

static void* do_drun(void* data) {
	(void) data;
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
			if(!G_IS_DESKTOP_APP_INFO(info) || g_desktop_app_info_get_is_hidden(info) || g_desktop_app_info_get_nodisplay(info)) {
				free(full_path);
				continue;
			}
			const char* name = g_app_info_get_display_name(G_APP_INFO(info));
			if(name == NULL) {
				free(full_path);
				continue;
			}
			char* text;
			GIcon* icon = g_app_info_get_icon(G_APP_INFO(info));
			if(allow_images && icon != NULL) {
				if(G_IS_THEMED_ICON(icon)) {
					GtkIconTheme* theme = gtk_icon_theme_get_default();
					const gchar* const* icon_names = g_themed_icon_get_names(G_THEMED_ICON(icon));
					GtkIconInfo* info = gtk_icon_theme_choose_icon(theme, (const gchar**) icon_names, 32, 0);
					const gchar* icon_path = gtk_icon_info_get_filename(info);
					text = utils_concat(4, "img:", icon_path, ":text:", name);
				}
			} else {
				text = strdup(name);
			}
			struct node* node = malloc(sizeof(struct node));
			node->text = text;
			node->action = strdup(full_path);
			node->container = GTK_CONTAINER(inner_box);
			g_idle_add(insert_widget, node);
			utils_sleep_millis(1);
			free(full_path);
		}
		closedir(dir);
		cont:
		dirs += strlen(dirs) + 1;
		free(app_dir);
	}
	free(original_dirs);
	return NULL;
}

static void execute_action(char* mode, const gchar* cmd) {
	char* cache_path = get_cache_path(mode);
	struct wl_list lines;
	wl_list_init(&lines);
	bool inc_count = false;
	if(access(cache_path, R_OK) == 0) {
		FILE* file = fopen(cache_path, "r");
		char* line = NULL;
		size_t size = 0;
		while(getline(&line, &size, file) != -1) {
			struct cache_line* node = malloc(sizeof(struct cache_line));
			if(strstr(line, cmd) != NULL) {
				uint64_t count = strtol(line, NULL, 10) + 1;
				char num[6];
				snprintf(num, 5, "%lu", count);
				node->line = utils_concat(4, num, " ", cmd, "\n");
				inc_count = true;
			} else {
				node->line = strdup(line);
			}
			wl_list_insert(&lines, &node->link);
		}
		free(line);
		fclose(file);
	}
	if(!inc_count) {
		struct cache_line* node = malloc(sizeof(struct cache_line));
		node->line = utils_concat(3, "1 ", cmd, "\n");
		wl_list_insert(&lines, &node->link);
	}

	FILE* file = fopen(cache_path, "w");
	struct cache_line* node, *tmp;
	wl_list_for_each_safe(node, tmp, &lines, link) {
		fwrite(node->line, 1, strlen(node->line), file);
		free(node->line);
		wl_list_remove(&node->link);
		free(node);
	}

	fclose(file);

	free(cache_path);
	if(strcmp(mode, "run") == 0) {
		execlp(cmd, cmd, NULL);
		fprintf(stderr, "%s cannot be executed\n", cmd);
		exit(errno);
	} else if(strcmp(mode, "dmenu") == 0) {
		printf("%s\n", cmd);
		exit(0);
	} else if(strcmp(mode, "drun") == 0) {
		GDesktopAppInfo* info = g_desktop_app_info_new_from_filename(cmd);
		if(G_IS_DESKTOP_APP_INFO(info)) {
			const char* exec = g_app_info_get_executable(G_APP_INFO(info));
			execlp(exec, exec, NULL);
			fprintf(stderr, "%s cannot be executed\n", exec);
			exit(errno);
		} else {
			fprintf(stderr, "%s cannot be executed\n", cmd);
			exit(1);
		}
	}
}

static void activate_item(GtkListBox* list_box, GtkListBoxRow* row, gpointer data) {
	(void) list_box;
	(void) data;
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
	execute_action(mode, wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "action"));
}

static void select_item(GtkListBox* list_box, GtkListBoxRow* row, gpointer data) {
	(void) list_box;
	(void) data;
	if(previous_selection != NULL) {
		gtk_widget_set_name(previous_selection, "unselected");
	}
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
	gtk_widget_set_name(box, "selected");
	previous_selection = box;
}

static void activate_search(GtkEntry* entry, gpointer data) {
	(void) data;
	if(strcmp(mode, "dmenu") == 0) {
		execute_action(mode, gtk_entry_get_text(entry));
	} else {
		GtkListBoxRow* row = gtk_list_box_get_row_at_y(GTK_LIST_BOX(inner_box), 0);
		GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
		execute_action(mode, wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "action"));
	}
}

static gboolean do_filter(GtkListBoxRow* row, gpointer data) {
	(void) data;
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
	const gchar* text = wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "filter");
	if(filter == NULL || strcmp(filter, "") == 0) {
		return TRUE;
	}
	if(strcasestr(text, filter) != NULL) {
		return TRUE;
	}
	return FALSE;
}

static gboolean key_press(GtkWidget* widget, GdkEvent* event, gpointer data) {
	(void) widget;
	(void) event;
	(void) data;
	guint code;
	gdk_event_get_keyval(event, &code);
	switch(code) {
	case GDK_KEY_Escape:
		exit(0);
		break;
	case GDK_KEY_Up:
	case GDK_KEY_Down:
	case GDK_KEY_Left:
	case GDK_KEY_Right:
	case GDK_KEY_Return:
		break;
	default:
		if(!gtk_widget_has_focus(entry)) {
			gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entry));
		}
		break;
	}
	return FALSE;
}

void wofi_init(struct map* config) {
	width = strtol(config_get(config, "width", "1000"), NULL, 10);
	height = strtol(config_get(config, "height", "400"), NULL, 10);
	x = strtol(config_get(config, "x", "-1"), NULL, 10);
	y = strtol(config_get(config, "y", "-1"), NULL, 10);
	bool normal_window = strcmp(config_get(config, "normal_window", "false"), "true") == 0;
	mode = map_get(config, "mode");
	char* prompt = config_get(config, "prompt", mode);
	filter_rate = strtol(config_get(config, "filter_rate", "100"), NULL, 10);
	filter_time = utils_get_time_millis();
	allow_images = strcmp(config_get(config, "allow_images", "false"), "true") == 0;
	allow_markup = strcmp(config_get(config, "allow_markup", "false"), "true") == 0;

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

	outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(outer_box, "outer-box");
	gtk_container_add(GTK_CONTAINER(window), outer_box);
	entry = gtk_search_entry_new();
	gtk_widget_set_name(entry, "input");
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), prompt);
	gtk_container_add(GTK_CONTAINER(outer_box), entry);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(scroll, "scroll");
	gtk_container_add(GTK_CONTAINER(outer_box), scroll);
	gtk_widget_set_size_request(scroll, width, height);

	inner_box = gtk_list_box_new();
	gtk_widget_set_name(inner_box, "inner-box");
	gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(inner_box), FALSE);
	gtk_container_add(GTK_CONTAINER(scroll), inner_box);

	gtk_list_box_set_filter_func(GTK_LIST_BOX(inner_box), do_filter, NULL, NULL);

	g_signal_connect(entry, "changed", G_CALLBACK(get_input), NULL);
	g_signal_connect(entry, "search-changed", G_CALLBACK(get_search), NULL);
	g_signal_connect(inner_box, "row-activated", G_CALLBACK(activate_item), NULL);
	g_signal_connect(inner_box, "row-selected", G_CALLBACK(select_item), NULL);
	g_signal_connect(entry, "activate", G_CALLBACK(activate_search), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(key_press), NULL);

	pthread_t thread;
	if(strcmp(mode, "run") == 0) {
		pthread_create(&thread, NULL, do_run, NULL);
	} else if(strcmp(mode, "dmenu") == 0) {
		pthread_create(&thread, NULL, do_dmenu, NULL);
	} else if(strcmp(mode, "drun") == 0) {
		pthread_create(&thread, NULL, do_drun, NULL);
	} else {
		fprintf(stderr, "I would love to show %s but Idk what it is\n", mode);
		exit(1);
	}
	gtk_widget_grab_focus(entry);
	gtk_window_set_title(GTK_WINDOW(window), prompt);
	gtk_widget_show_all(window);
}
