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
static uint64_t image_size;
static char* cache_file = NULL;
static char* config_dir;
static void (*mode_exec)(const gchar* cmd);
static bool (*exec_search)();

struct node {
	char* text, *action;
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
		gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(inner_box));
	}
}

static void get_search(GtkSearchEntry* entry, gpointer data) {
	(void) data;
	filter = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(inner_box));
}

static GtkWidget* create_label(char* text, char* action) {
	GtkWidget* box = wofi_property_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(box, "unselected");
	GtkStyleContext* style = gtk_widget_get_style_context(box);
	gtk_style_context_add_class(style, "entry");
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
						float percent = (float) image_size / height;
						GdkPixbuf* tmp = gdk_pixbuf_scale_simple(buf, width * percent, image_size, GDK_INTERP_BILINEAR);
						g_object_unref(buf);
						buf = tmp;
					} else {
						float percent = (float) image_size / width;
						GdkPixbuf* tmp = gdk_pixbuf_scale_simple(buf, image_size, height * percent, GDK_INTERP_BILINEAR);
						g_object_unref(buf);
						buf = tmp;
					}
					GtkWidget* img = gtk_image_new_from_pixbuf(buf);
					gtk_widget_set_name(img, "img");
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
					gtk_widget_set_name(label, "text");
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

static gboolean _insert_widget(gpointer data) {
	struct node* node = data;
	GtkWidget* box = create_label(node->text, node->action);
	gtk_container_add(GTK_CONTAINER(inner_box), box);
	gtk_widget_show_all(box);

	free(node->text);
	free(node->action);
	free(node);
	return FALSE;
}

static char* get_cache_path(char* mode) {
	if(cache_file != NULL) {
		return strdup(cache_file);
	}
	char* cache_path = getenv("XDG_CACHE_HOME");
	if(cache_path == NULL) {
		cache_path = utils_concat(3, getenv("HOME"), "/.cache/wofi-", mode);
	} else {
		cache_path = utils_concat(3, cache_path, "/wofi-", mode);
	}
	return cache_path;
}

struct wl_list* wofi_read_cache(char* mode) {
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
		uint64_t smallest = UINT64_MAX;
		struct cache_line* node, *smallest_node = NULL;
		wl_list_for_each(node, &lines, link) {
			uint64_t num = strtol(node->line, NULL, 10);
			if(num < smallest) {
				smallest = num;
				smallest_node = node;
			}
		}
		char* tmp = strdup(strchr(smallest_node->line, ' ') + 1);
		free(smallest_node->line);
		smallest_node->line = tmp;
		wl_list_remove(&smallest_node->link);
		wl_list_insert(cache, &smallest_node->link);
	}
	free(cache_path);
	return cache;
}

void wofi_insert_widget(char* text, char* action) {
	struct node* widget = malloc(sizeof(struct node));
	widget->text = strdup(text);
	widget->action = strdup(action);
	g_idle_add(_insert_widget, widget);
	utils_sleep_millis(1);
}

bool wofi_allow_images() {
	return allow_images;
}

uint64_t wofi_get_image_size() {
	return image_size;
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
	mode_exec(cmd);
}

static void activate_item(GtkFlowBox* flow_box, GtkFlowBoxChild* row, gpointer data) {
	(void) flow_box;
	(void) data;
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
	execute_action(mode, wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "action"));
}

static void select_item(GtkFlowBox* flow_box, gpointer data) {
	(void) data;
	if(previous_selection != NULL) {
		gtk_widget_set_name(previous_selection, "unselected");
	}
	GList* selected_children = gtk_flow_box_get_selected_children(flow_box);
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(selected_children->data));
	g_list_free(selected_children);
	gtk_widget_set_name(box, "selected");
	previous_selection = box;
}

static void activate_search(GtkEntry* entry, gpointer data) {
	(void) data;
	if(exec_search != NULL && exec_search()) {
		execute_action(mode, gtk_entry_get_text(entry));
	} else {
		GtkFlowBoxChild* row = gtk_flow_box_get_child_at_pos(GTK_FLOW_BOX(inner_box), 0, 0);
		GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
		execute_action(mode, wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "action"));
	}
}

static gboolean do_filter(GtkFlowBoxChild* row, gpointer data) {
	(void) data;
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
	const gchar* text = wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "filter");
	if(filter == NULL || strcmp(filter, "") == 0) {
		return TRUE;
	}
	if(text == NULL) {
		return FALSE;
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
	case GDK_KEY_Left:
	case GDK_KEY_Right:
	case GDK_KEY_Return:
		break;
	case GDK_KEY_Down:
		if(gtk_widget_has_focus(entry)) {
			GtkFlowBoxChild* child = gtk_flow_box_get_child_at_pos(GTK_FLOW_BOX(inner_box), 0, 0);
			gtk_widget_grab_focus(GTK_WIDGET(child));
			gtk_flow_box_select_child(GTK_FLOW_BOX(inner_box), child);
			return TRUE;
		}
		break;
	default:
		if(!gtk_widget_has_focus(entry)) {
			gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entry));
		}
		break;
	}
	return FALSE;
}

static void* get_plugin_proc(const char* prefix, const char* suffix) {
	char* proc_name = utils_concat(2, prefix, suffix);
	void* proc = dlsym(RTLD_DEFAULT, proc_name);
	free(proc_name);
	return proc;
}

static void* start_thread(void* data) {
	char* mode = data;
	char* dso = strstr(mode, ".so");

	void (*init)();
	if(dso == NULL) {
		init = get_plugin_proc(mode, "_init");
		mode_exec = get_plugin_proc(mode, "_exec");
		exec_search = get_plugin_proc(mode, "_exec_search");
	} else {
		char* plugins_dir = utils_concat(2, config_dir, "/plugins/");
		char* full_name = utils_concat(2, plugins_dir, mode);
		void* plugin = dlopen(full_name, RTLD_LAZY);
		free(full_name);
		free(plugins_dir);
		init = dlsym(plugin, "init");
		mode_exec = dlsym(plugin, "exec");
		exec_search = dlsym(plugin, "exec_search");
	}

	if(init != NULL) {
		init();
	} else {
		fprintf(stderr, "I would love to show %s but Idk what it is\n", mode);
		exit(1);
	}

	return NULL;
}

void wofi_init(struct map* config) {
	width = strtol(config_get(config, "width", "1000"), NULL, 10);
	height = strtol(config_get(config, "height", "400"), NULL, 10);
	x = strtol(config_get(config, "x", "-1"), NULL, 10);
	y = strtol(config_get(config, "y", "-1"), NULL, 10);
	bool normal_window = strcmp(config_get(config, "normal_window", "false"), "true") == 0;
	mode = map_get(config, "mode");
	GtkOrientation orientation = strcmp(config_get(config, "orientation", "vertical"), "horizontal") == 0 ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
	uint8_t halign = config_get_mnemonic(config, "halign", "fill", 4, "fill", "start", "end", "center");
	uint8_t valign = config_get_mnemonic(config, "valign", "start", 4, "fill", "start", "end", "center");
	char* prompt = config_get(config, "prompt", mode);
	filter_rate = strtol(config_get(config, "filter_rate", "100"), NULL, 10);
	filter_time = utils_get_time_millis();
	allow_images = strcmp(config_get(config, "allow_images", "false"), "true") == 0;
	allow_markup = strcmp(config_get(config, "allow_markup", "false"), "true") == 0;
	image_size = strtol(config_get(config, "image_size", "32"), NULL, 10);
	cache_file = map_get(config, "cache_file");
	config_dir = map_get(config, "config_dir");

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

	inner_box = gtk_flow_box_new();
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(inner_box), 1);
	gtk_orientable_set_orientation(GTK_ORIENTABLE(inner_box), orientation);
	gtk_widget_set_halign(inner_box, halign);
	gtk_widget_set_valign(inner_box, valign);

	gtk_widget_set_name(inner_box, "inner-box");
	gtk_flow_box_set_activate_on_single_click(GTK_FLOW_BOX(inner_box), FALSE);
	gtk_container_add(GTK_CONTAINER(scroll), inner_box);

	gtk_flow_box_set_filter_func(GTK_FLOW_BOX(inner_box), do_filter, NULL, NULL);

	g_signal_connect(entry, "changed", G_CALLBACK(get_input), NULL);
	g_signal_connect(entry, "search-changed", G_CALLBACK(get_search), NULL);
	g_signal_connect(inner_box, "child-activated", G_CALLBACK(activate_item), NULL);
	g_signal_connect(inner_box, "selected-children-changed", G_CALLBACK(select_item), NULL);
	g_signal_connect(entry, "activate", G_CALLBACK(activate_search), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(key_press), NULL);

	pthread_t thread;
	pthread_create(&thread, NULL, start_thread, mode);
	gtk_widget_grab_focus(entry);
	gtk_window_set_title(GTK_WINDOW(window), prompt);
	gtk_widget_show_all(window);
}
