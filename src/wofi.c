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

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <utils.h>
#include <config.h>
#include <utils_g.h>
#include <property_box.h>
#include <widget_builder.h>

#include <xdg-output-unstable-v1-client-protocol.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>

#include <pango/pango.h>
#include <gdk/gdkwayland.h>

static const char* terminals[] = {"kitty", "termite", "alacritty", "gnome-terminal", "weston-terminal"};

enum matching_mode {
	MATCHING_MODE_CONTAINS,
	MATCHING_MODE_FUZZY
};

enum location {
	LOCATION_CENTER,
	LOCATION_TOP_LEFT,
	LOCATION_TOP,
	LOCATION_TOP_RIGHT,
	LOCATION_RIGHT,
	LOCATION_BOTTOM_RIGHT,
	LOCATION_BOTTOM,
	LOCATION_BOTTOM_LEFT,
	LOCATION_LEFT
};

enum sort_order {
	SORT_ORDER_DEFAULT,
	SORT_ORDER_ALPHABETICAL
};

static uint64_t width, height;
static char* x, *y;
static struct zwlr_layer_shell_v1* shell = NULL;
static GtkWidget* window, *outer_box, *scroll, *entry, *inner_box, *previous_selection = NULL;
static gchar* filter = NULL;
static char* mode = NULL;
static bool allow_images, allow_markup;
static uint64_t image_size;
static char* cache_file = NULL;
static char* config_dir;
static bool mod_shift;
static bool mod_ctrl;
static char* terminal;
static GtkOrientation outer_orientation;
static bool exec_search;
static struct map* modes;
static enum matching_mode matching;
static bool insensitive;
static bool parse_search;
static GtkAlign content_halign;
static struct map* config;
static enum location location;
static bool no_actions;
static uint64_t columns;
static bool user_moved = false;
static uint32_t widget_count = 0;
static enum sort_order sort_order;
static int64_t max_height = 0;
static uint32_t lines, max_lines;
static int8_t line_wrap;
static int64_t ix, iy;
static uint8_t konami_cycle;
static bool is_konami = false;
static GDBusProxy* dbus = NULL;
static GdkRectangle resolution = {0};
static bool resize_expander = false;
static uint32_t line_count = 0;
static bool dynamic_lines;
static struct wl_list mode_list;
static pthread_t mode_thread;
static bool has_joined_mode = false;
static char* copy_exec = NULL;

static struct map* keys;

static struct wl_display* wl = NULL;
static struct wl_surface* wl_surface;
static struct wl_list outputs;
static struct zxdg_output_manager_v1* output_manager;
static struct zwlr_layer_surface_v1* wlr_surface;

struct output_node {
	char* name;
	struct wl_output* output;
	int32_t width, height, x, y;
	struct wl_list link;
};

struct key_entry {
	char* mod;
	void (*action)(void);
};

static void nop() {}

static void add_interface(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
	(void) data;
	if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version);
	} else if(strcmp(interface, wl_output_interface.name) == 0) {
		struct output_node* node = malloc(sizeof(struct output_node));
		node->output = wl_registry_bind(registry, name, &wl_output_interface, version);
		wl_list_insert(&outputs, &node->link);
	} else if(strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
		output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, version);
	}
}

static void config_surface(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height) {
	(void) data;
	(void) width;
	(void) height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void setup_surface(struct zwlr_layer_surface_v1* surface) {
	zwlr_layer_surface_v1_set_size(surface, width, height);
	zwlr_layer_surface_v1_set_keyboard_interactivity(surface, true);

	if(location > 8) {
		location -= 9;
	}

	if(x != NULL || y != NULL) {
		if(location == LOCATION_CENTER) {
			location = LOCATION_TOP_LEFT;
		}

		zwlr_layer_surface_v1_set_margin(surface, iy, -ix, -iy, ix);
	}

	if(location > 0) {
		enum zwlr_layer_surface_v1_anchor anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;

		switch(location) {
		case LOCATION_CENTER:
			break;
		case LOCATION_TOP_LEFT:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
			break;
		case LOCATION_TOP:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
			break;
		case LOCATION_TOP_RIGHT:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			break;
		case LOCATION_LEFT:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
			break;
		case LOCATION_RIGHT:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			break;
		case LOCATION_BOTTOM_LEFT:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
			break;
		case LOCATION_BOTTOM:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
			break;
		case LOCATION_BOTTOM_RIGHT:
			anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			break;
		}
		zwlr_layer_surface_v1_set_anchor(surface, anchor);
	}
}

static gboolean do_search(gpointer data) {
	(void) data;
	const gchar* new_filter = gtk_entry_get_text(GTK_ENTRY(entry));
	if(filter == NULL || strcmp(new_filter, filter) != 0) {
		if(filter != NULL) {
			free(filter);
		}
		filter = strdup(new_filter);
		gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(inner_box));
		gtk_flow_box_invalidate_sort(GTK_FLOW_BOX(inner_box));
		GtkFlowBoxChild* child = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(inner_box), 0);
		if(child != NULL) {
			gtk_flow_box_select_child(GTK_FLOW_BOX(inner_box), child);
		}
	}
	return G_SOURCE_CONTINUE;
}

static void get_img_data(char* original, char* str, struct map* mode_map, bool first, char** mode, char** data) {
	char* colon = strchr(str, ':');
	if(colon == NULL) {
		if(first) {
			*mode = "text";
			*data = str;
			return;
		} else {
			*mode = NULL;
			*data = NULL;
			return;
		}
	}

	*colon = 0;

	if(map_contains(mode_map, str)) {
		if(original != str) {
			*(str - 1) = 0;
		}
		*mode = str;
		*data = colon + 1;
	} else if(first) {
		*colon = ':';
		*mode = "text";
		*data = str;
	} else {
		*colon = ':';
		get_img_data(original, colon + 1, mode_map, first, mode, data);
	}
}

//This is hideous, why did I do this to myself
static char* parse_images(WofiPropertyBox* box, const char* text, bool create_widgets) {
	char* ret = strdup("");
	struct map* mode_map = map_init();
	map_put(mode_map, "img", "true");
	map_put(mode_map, "img-noscale", "true");
	map_put(mode_map, "img-base64", "true");
	map_put(mode_map, "img-base64-noscale", "true");
	map_put(mode_map, "text", "true");

	char* original = strdup(text);
	char* mode1 = NULL;
	char* mode2 = NULL;
	char* data1 = NULL;
	char* data2 = NULL;

	get_img_data(original, original, mode_map, true, &mode2, &data2);

	while(true) {
		if(mode1 == NULL) {
			mode1 = mode2;
			data1 = data2;
			if(data1 != NULL) {
				get_img_data(original, data1, mode_map, false, &mode2, &data2);
			} else {
				break;
			}
		} else {
			if(strcmp(mode1, "img") == 0 && create_widgets) {
				GdkPixbuf* buf = gdk_pixbuf_new_from_file(data1, NULL);
				if(buf == NULL) {
					fprintf(stderr, "Image %s cannot be loaded\n", data1);
					goto done;
				}

				buf = utils_g_resize_pixbuf(buf, image_size * wofi_get_window_scale(), GDK_INTERP_BILINEAR);

				GtkWidget* img = gtk_image_new();
				cairo_surface_t* surface = gdk_cairo_surface_create_from_pixbuf(buf, wofi_get_window_scale(), gtk_widget_get_window(img));
				gtk_image_set_from_surface(GTK_IMAGE(img), surface);
				cairo_surface_destroy(surface);
				g_object_unref(buf);

				gtk_widget_set_name(img, "img");
				gtk_container_add(GTK_CONTAINER(box), img);
			} else if(strcmp(mode1, "img-noscale") == 0 && create_widgets) {
				GdkPixbuf* buf = gdk_pixbuf_new_from_file(data1, NULL);
				if(buf == NULL) {
					fprintf(stderr, "Image %s cannot be loaded\n", data1);
					goto done;
				}

				GtkWidget* img = gtk_image_new();
				cairo_surface_t* surface = gdk_cairo_surface_create_from_pixbuf(buf, wofi_get_window_scale(), gtk_widget_get_window(img));
				gtk_image_set_from_surface(GTK_IMAGE(img), surface);
				cairo_surface_destroy(surface);
				g_object_unref(buf);

				gtk_widget_set_name(img, "img");
				gtk_container_add(GTK_CONTAINER(box), img);
			} else if(strcmp(mode1, "img-base64") == 0 && create_widgets) {
				GdkPixbuf* buf = utils_g_pixbuf_from_base64(data1);
				if(buf == NULL) {
					fprintf(stderr, "base64 image cannot be loaded\n");
					goto done;
				}

				buf = utils_g_resize_pixbuf(buf, image_size, GDK_INTERP_BILINEAR);

				GtkWidget* img = gtk_image_new();
				cairo_surface_t* surface = gdk_cairo_surface_create_from_pixbuf(buf, wofi_get_window_scale(), gtk_widget_get_window(img));
				gtk_image_set_from_surface(GTK_IMAGE(img), surface);
				cairo_surface_destroy(surface);
				g_object_unref(buf);

				gtk_widget_set_name(img, "img");
				gtk_container_add(GTK_CONTAINER(box), img);
			} else if(strcmp(mode1, "img-base64-noscale") == 0 && create_widgets) {
				GdkPixbuf* buf = utils_g_pixbuf_from_base64(data1);
				if(buf == NULL) {
					fprintf(stderr, "base64 image cannot be loaded\n");
					goto done;
				}
				GtkWidget* img = gtk_image_new();
				cairo_surface_t* surface = gdk_cairo_surface_create_from_pixbuf(buf, wofi_get_window_scale(), gtk_widget_get_window(img));
				gtk_image_set_from_surface(GTK_IMAGE(img), surface);
				cairo_surface_destroy(surface);
				g_object_unref(buf);

				gtk_widget_set_name(img, "img");
				gtk_container_add(GTK_CONTAINER(box), img);
			} else if(strcmp(mode1, "text") == 0) {
				if(create_widgets) {
					GtkWidget* label = gtk_label_new(data1);
					gtk_widget_set_name(label, "text");
					gtk_label_set_use_markup(GTK_LABEL(label), allow_markup);
					gtk_label_set_xalign(GTK_LABEL(label), 0);
					if(line_wrap >= 0) {
						gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
						gtk_label_set_line_wrap_mode(GTK_LABEL(label), line_wrap);
					}
					gtk_container_add(GTK_CONTAINER(box), label);
				} else {
					char* tmp = ret;
					ret = utils_concat(2, ret, data1);
					free(tmp);
				}
			}
			done:
			mode1 = NULL;
		}
	}
	free(original);
	map_free(mode_map);

	if(create_widgets) {
		free(ret);
		return NULL;
	} else {
		return ret;
	}
}

char* wofi_parse_image_escapes(const char* text) {
	return parse_images(NULL, text, false);
}

static void setup_label(char* mode, WofiPropertyBox* box) {
	wofi_property_box_add_property(box, "mode", mode);
	char index[11];
	snprintf(index, sizeof(index), "%u", ++widget_count);
	wofi_property_box_add_property(box, "index", index);

	gtk_widget_set_name(GTK_WIDGET(box), "unselected");

	GtkStyleContext* style = gtk_widget_get_style_context(GTK_WIDGET(box));
	gtk_style_context_add_class(style, "entry");
}

static GtkWidget* create_label(char* mode, char* text, char* search_text, char* action) {
	GtkWidget* box = wofi_property_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	wofi_property_box_add_property(WOFI_PROPERTY_BOX(box), "action", action);

	setup_label(mode, WOFI_PROPERTY_BOX(box));

	if(allow_images) {
		parse_images(WOFI_PROPERTY_BOX(box), text, true);
	} else {
		GtkWidget* label = gtk_label_new(text);
		gtk_widget_set_name(label, "text");
		gtk_label_set_use_markup(GTK_LABEL(label), allow_markup);
		gtk_label_set_xalign(GTK_LABEL(label), 0);
		if(line_wrap >= 0) {
			gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
			gtk_label_set_line_wrap_mode(GTK_LABEL(label), line_wrap);
		}
		gtk_container_add(GTK_CONTAINER(box), label);
	}
	if(parse_search) {
		search_text = strdup(search_text);
		if(allow_images) {
			char* tmp = search_text;
			search_text = parse_images(WOFI_PROPERTY_BOX(box), search_text, false);
			free(tmp);
		}
		if(allow_markup) {
			char* out = NULL;
			pango_parse_markup(search_text, -1, 0, NULL, &out, NULL, NULL);
			free(search_text);
			search_text = out;
		}
	}
	wofi_property_box_add_property(WOFI_PROPERTY_BOX(box), "filter", search_text);
	if(parse_search) {
		free(search_text);
	}
	return box;
}

static char* get_cache_path(const gchar* mode) {
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

static void execute_action(const gchar* mode, const gchar* cmd) {
	struct mode* mode_ptr = map_get(modes, mode);
	mode_ptr->mode_exec(cmd);
}

static void activate_item(GtkFlowBox* flow_box, GtkFlowBoxChild* row, gpointer data) {
	(void) flow_box;
	(void) data;
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
	bool primary_action = GTK_IS_EXPANDER(box);
	if(primary_action) {
		box = gtk_expander_get_label_widget(GTK_EXPANDER(box));
	}
	execute_action(wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "mode"), wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "action"));
}

static void expand(GtkExpander* expander, gpointer data) {
	(void) data;
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(expander));
	resize_expander = !gtk_expander_get_expanded(expander);
	gtk_widget_set_visible(box, resize_expander);
}

static void update_surface_size(void) {
	if(lines > 0) {
		height = max_height * lines;
		height += 5;
	}
	if(shell != NULL) {
		zwlr_layer_surface_v1_set_size(wlr_surface, width, height);
		wl_surface_commit(wl_surface);
		wl_display_roundtrip(wl);
	}

	gtk_window_resize(GTK_WINDOW(window), width, height);
	gtk_widget_set_size_request(scroll, width, height);
}

static void widget_allocate(GtkWidget* widget, GdkRectangle* allocation, gpointer data) {
	(void) data;
	if(resize_expander) {
		return;
	}

	if(max_height > 0) {
		if(allocation->height > max_height) {
			int64_t ratio = allocation->height / max_height;
			int64_t mod = allocation->height % max_height;
			if(mod >= max_height / 2) {
				++ratio;
			}
			if(ratio > 1) {
				gtk_widget_set_size_request(widget, -1, max_height * ratio);
			} else {
				max_height = allocation->height;
			}
		} else {
			gtk_widget_set_size_request(widget, -1, max_height);
		}
	} else {
		max_height = allocation->height;
	}
	if(lines > 0) {
		update_surface_size();
	}
}

static gboolean _insert_widget(gpointer data) {
	struct mode* mode = data;
	struct widget* node;
	if(mode->mode_get_widget == NULL) {
		return FALSE;
	} else {
		node = mode->mode_get_widget();
	}
	if(node == NULL) {
		return FALSE;
	}

	GtkWidget* parent;
	if(node->action_count > 1 && !no_actions) {
		parent = gtk_expander_new("");
		g_signal_connect(parent, "activate", G_CALLBACK(expand), NULL);
		GtkWidget* box;
		if(node->builder == NULL) {
			box = create_label(node->mode, node->text[0], node->search_text, node->actions[0]);
		} else {
			box = GTK_WIDGET(node->builder->box);
			setup_label(node->builder->mode->name, WOFI_PROPERTY_BOX(box));
		}
		gtk_expander_set_label_widget(GTK_EXPANDER(parent), box);

		GtkWidget* exp_box = gtk_list_box_new();
		gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(exp_box), FALSE);
		g_signal_connect(exp_box, "row-activated", G_CALLBACK(activate_item), NULL);
		gtk_container_add(GTK_CONTAINER(parent), exp_box);
		for(size_t count = 1; count < node->action_count; ++count) {
			if(node->builder == NULL) {
				box = create_label(node->mode, node->text[count], node->search_text, node->actions[count]);
			} else {
				box = GTK_WIDGET(node->builder[count].box);
				setup_label(node->builder->mode->name, WOFI_PROPERTY_BOX(box));
			}

			GtkWidget* row = gtk_list_box_row_new();
			gtk_widget_set_name(row, "entry");

			gtk_container_add(GTK_CONTAINER(row), box);
			gtk_container_add(GTK_CONTAINER(exp_box), row);
		}
	} else {
		if(node->builder == NULL) {
			parent = create_label(node->mode, node->text[0], node->search_text, node->actions[0]);
		} else {
			parent = GTK_WIDGET(node->builder->box);
			setup_label(node->builder->mode->name, WOFI_PROPERTY_BOX(parent));
		}
	}

	gtk_widget_set_halign(parent, content_halign);
	GtkWidget* child = gtk_flow_box_child_new();
	gtk_widget_set_name(child, "entry");
	g_signal_connect(child, "size-allocate", G_CALLBACK(widget_allocate), NULL);

	gtk_container_add(GTK_CONTAINER(child), parent);
	gtk_widget_show_all(child);
	gtk_container_add(GTK_CONTAINER(inner_box), child);
	++line_count;

	if(!user_moved) {
		GtkFlowBoxChild* child = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(inner_box), 0);
		gtk_flow_box_select_child(GTK_FLOW_BOX(inner_box), child);
		gtk_widget_grab_focus(GTK_WIDGET(child));
	}

	if(GTK_IS_EXPANDER(parent)) {
		GtkWidget* box = gtk_bin_get_child(GTK_BIN(parent));
		gtk_widget_set_visible(box, FALSE);
	}

	if(node->builder != NULL) {
		wofi_widget_builder_free(node->builder);
	} else {
		free(node->mode);
		for(size_t count = 0; count < node->action_count; ++count) {
			free(node->text[count]);
		}
		free(node->text);
		free(node->search_text);
		for(size_t count = 0; count < node->action_count; ++count) {
			free(node->actions[count]);
		}
		free(node->actions);
		free(node);
	}
	return TRUE;
}

static gboolean insert_all_widgets(gpointer data) {
	if(!has_joined_mode) {
		pthread_join(mode_thread, NULL);
		has_joined_mode = true;
	}
	struct wl_list* modes = data;
	if(modes->prev == modes) {
		return FALSE;
	} else {
		struct mode* mode = wl_container_of(modes->prev, mode, link);
		if(!_insert_widget(mode)) {
			wl_list_remove(&mode->link);
		}
		return TRUE;
	}
}

static char* escape_lf(const char* cmd) {
	size_t len = strlen(cmd);
	char* buffer = calloc(1, (len + 1) * 2);

	size_t buf_count = 0;
	for(size_t count = 0; count < len; ++count) {
		char chr = cmd[count];
		if(chr == '\n') {
			buffer[buf_count++] = '\\';
			buffer[buf_count++] = 'n';
		} else if(chr == '\\') {
			buffer[buf_count++] = '\\';
			buffer[buf_count++] = '\\';
		} else {
			buffer[buf_count++] = chr;
		}
	}
	return buffer;
}

static char* remove_escapes(const char* cmd) {
	size_t len = strlen(cmd);
	char* buffer = calloc(1, len + 1);

	size_t buf_count = 0;
	for(size_t count = 0; count < len; ++count) {
		char chr = cmd[count];
		if(chr == '\\') {
			chr = cmd[++count];
			if(chr == 'n') {
				buffer[buf_count++] = '\n';
			} else if(chr == '\\') {
				buffer[buf_count++] = '\\';
			}
		} else {
			buffer[buf_count++] = chr;
		}
	}
	return buffer;
}

void wofi_write_cache(struct mode* mode, const char* _cmd) {
	char* cmd = escape_lf(_cmd);

	char* cache_path = get_cache_path(mode->name);

	char* tmp_dir = strdup(cache_path);
	utils_mkdir(dirname(tmp_dir), S_IRWXU | S_IRGRP | S_IXGRP);
	free(tmp_dir);

	struct wl_list lines;
	wl_list_init(&lines);
	bool inc_count = false;
	if(access(cache_path, R_OK) == 0) {
		FILE* file = fopen(cache_path, "r");
		char* line = NULL;
		size_t size = 0;
		while(getline(&line, &size, file) != -1) {
			char* space = strchr(line, ' ');
			char* lf = strchr(line, '\n');
			if(lf != NULL) {
				*lf = 0;
			}
			if(space != NULL && strcmp(cmd, space + 1) == 0) {
				struct cache_line* node = malloc(sizeof(struct cache_line));
				uint64_t count = strtol(line, NULL, 10) + 1;
				char num[6];
				snprintf(num, 5, "%" PRIu64, count);
				node->line = utils_concat(4, num, " ", cmd, "\n");
				inc_count = true;
				wl_list_insert(&lines, &node->link);
			}
		}
		free(line);
		line = NULL;
		size = 0;
		fseek(file, 0, SEEK_SET);
		while(getline(&line, &size, file) != -1) {
			char* space = strchr(line, ' ');
			char* nl = strchr(line, '\n');
			if(nl != NULL) {
				*nl = 0;
			}
			if(space == NULL || strcmp(cmd, space + 1) != 0) {
				struct cache_line* node = malloc(sizeof(struct cache_line));
				node->line = utils_concat(2, line, "\n");
				wl_list_insert(&lines, &node->link);
			}
		}
		free(line);
		fclose(file);
	}
	char* tmp_path = strdup(cache_path);
	char* dir = dirname(tmp_path);

	if(access(dir, W_OK) == 0) {
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
	}
	free(cache_path);
	free(tmp_path);
	free(cmd);
}

void wofi_remove_cache(struct mode* mode, const char* _cmd) {
	char* cmd = escape_lf(_cmd);

	char* cache_path = get_cache_path(mode->name);
	if(access(cache_path, R_OK | W_OK) == 0) {
		struct wl_list lines;
		wl_list_init(&lines);

		FILE* file = fopen(cache_path, "r");
		char* line = NULL;
		size_t size = 0;
		while(getline(&line, &size, file) != -1) {
			char* space = strchr(line, ' ');
			char* lf = strchr(line, '\n');
			if(lf != NULL) {
				*lf = 0;
			}
			if(space == NULL || strcmp(cmd, space + 1) != 0) {
				struct cache_line* node = malloc(sizeof(struct cache_line));
				node->line = utils_concat(2, line, "\n");
				wl_list_insert(&lines, &node->link);
			}
		}
		free(line);
		fclose(file);

		file = fopen(cache_path, "w");
		struct cache_line* node, *tmp;
		wl_list_for_each_safe(node, tmp, &lines, link) {
			fwrite(node->line, 1, strlen(node->line), file);
			free(node->line);
			wl_list_remove(&node->link);
			free(node);
		}
		fclose(file);
	}
	free(cache_path);
	free(cmd);
}

struct wl_list* wofi_read_cache(struct mode* mode) {
	char* cache_path = get_cache_path(mode->name);
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
			node->line = remove_escapes(line);
			wl_list_insert(&lines, &node->link);
		}
		free(line);
		fclose(file);
	}
	while(!wl_list_empty(&lines)) {
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

struct widget* wofi_create_widget(struct mode* mode, char* text[], char* search_text, char* actions[], size_t action_count) {
	struct widget* widget = calloc(1, sizeof(struct widget));
	widget->mode = strdup(mode->name);
	widget->text = malloc(action_count * sizeof(char*));
	for(size_t count = 0; count < action_count; ++count) {
		widget->text[count] = strdup(text[count]);
	}
	widget->search_text = strdup(search_text);
	widget->action_count = action_count;
	widget->actions = malloc(action_count * sizeof(char*));
	for(size_t count = 0; count < action_count; ++count) {
		widget->actions[count] = strdup(actions[count]);
	}
	return widget;
}

void wofi_insert_widgets(struct mode* mode) {
	gdk_threads_add_idle(_insert_widget, mode);
}

char* wofi_get_dso_path(struct mode* mode) {
	return mode->dso;
}

bool wofi_allow_images(void) {
	return allow_images;
}

bool wofi_allow_markup(void) {
	return allow_markup;
}

uint64_t wofi_get_image_size(void) {
	return image_size;
}

uint64_t wofi_get_window_scale(void) {
	return gdk_window_get_scale_factor(gtk_widget_get_window(window));
}

bool wofi_mod_shift(void) {
	return mod_shift;
}

bool wofi_mod_control(void) {
	return mod_ctrl;
}

void wofi_term_run(const char* cmd) {
	if(terminal != NULL) {
		execlp(terminal, terminal, "-e", cmd, NULL);
	}
	size_t term_count = sizeof(terminals) / sizeof(char*);
	for(size_t count = 0; count < term_count; ++count) {
		execlp(terminals[count], terminals[count], "-e", cmd, NULL);
	}
	fprintf(stderr, "No terminal emulator found please set term in config or use --term\n");
	exit(1);
}

static void flag_box(GtkBox* box, GtkStateFlags flags) {
	GList* selected_children = gtk_container_get_children(GTK_CONTAINER(box));
	for(GList* list = selected_children; list != NULL; list = list->next) {
		GtkWidget* child = list->data;
		gtk_widget_set_state_flags(child, flags, TRUE);
	}
	g_list_free(selected_children);
}

static void select_item(GtkFlowBox* flow_box, gpointer data) {
	(void) data;
	if(previous_selection != NULL) {
		gtk_widget_set_name(previous_selection, "unselected");
		flag_box(GTK_BOX(previous_selection), GTK_STATE_FLAG_NORMAL);
	}
	GList* selected_children = gtk_flow_box_get_selected_children(flow_box);
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(selected_children->data));
	g_list_free(selected_children);
	if(GTK_IS_EXPANDER(box)) {
		box = gtk_expander_get_label_widget(GTK_EXPANDER(box));
	}
	flag_box(GTK_BOX(box), GTK_STATE_FLAG_SELECTED);
	gtk_widget_set_name(box, "selected");
	previous_selection = box;
}

static void activate_search(GtkEntry* entry, gpointer data) {
	(void) data;
	GtkFlowBoxChild* child = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(inner_box), 0);
	gboolean is_visible = gtk_widget_get_visible(GTK_WIDGET(child));
	if(mode != NULL && (exec_search || child == NULL || !is_visible)) {
		execute_action(mode, gtk_entry_get_text(entry));
	} else if(child != NULL) {
		GtkWidget* box = gtk_bin_get_child(GTK_BIN(child));
		bool primary_action = GTK_IS_EXPANDER(box);
		if(primary_action) {
			box = gtk_expander_get_label_widget(GTK_EXPANDER(box));
		}
		execute_action(wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "mode"), wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "action"));
	}
}

static gboolean filter_proxy(GtkFlowBoxChild* row) {
	GtkWidget* box = gtk_bin_get_child(GTK_BIN(row));
	if(GTK_IS_EXPANDER(box)) {
		box = gtk_expander_get_label_widget(GTK_EXPANDER(box));
	}
	const gchar* text = wofi_property_box_get_property(WOFI_PROPERTY_BOX(box), "filter");
	if(filter == NULL || strcmp(filter, "") == 0) {
		return TRUE;
	}
	if(text == NULL) {
		return FALSE;
	}
	if(insensitive) {
		return strcasestr(text, filter) != NULL;
	} else {
		return strstr(text, filter) != NULL;
	}
}

static gboolean do_filter(GtkFlowBoxChild* row, gpointer data) {
	(void) data;
	gboolean ret = filter_proxy(row);

	if(gtk_widget_get_visible(GTK_WIDGET(row)) == !ret && dynamic_lines) {
		if(ret) {
			++line_count;
		} else {
			--line_count;
		}

		if(line_count < max_lines) {
			lines = line_count;
			update_surface_size();
		} else {
			lines = max_lines;
			update_surface_size();
		}
	}

	gtk_widget_set_visible(GTK_WIDGET(row), ret);

	return ret;
}

static gint fuzzy_sort(const gchar* text1, const gchar* text2) {
	char* _filter = strdup(filter);
	size_t len = strlen(_filter);

	char* t1 = strdup(text1);
	size_t t1l = strlen(t1);

	char* t2 = strdup(text2);
	size_t t2l = strlen(t2);

	if(insensitive) {
		for(size_t count = 0; count < len; ++count) {
			char chr = _filter[count];
			if(isalpha(chr)) {
				_filter[count] = tolower(chr);
			}
		}
		for(size_t count = 0; count < t1l; ++count) {
			char chr = t1[count];
			if(isalpha(chr)) {
				t1[count] = tolower(chr);
			}
		}
		for(size_t count = 0; count < t2l; ++count) {
			char chr = t2[count];
			if(isalpha(chr)) {
				t2[count] = tolower(chr);
			}
		}
	}

	size_t dist1 = utils_distance(t1, _filter);
	size_t dist2 = utils_distance(t2, _filter);
	free(_filter);
	free(t1);
	free(t2);
	return dist1 - dist2;
}

static gint contains_sort(const gchar* text1, const gchar* text2) {
	char* str1, *str2;

	if(insensitive) {
		str1 = strcasestr(text1, filter);
		str2 = strcasestr(text2, filter);
	} else {
		str1 = strstr(text1, filter);
		str2 = strstr(text2, filter);
	}
	bool tx1 = str1 == text1;
	bool tx2 = str2 == text2;
	bool txc1 = str1 != NULL;
	bool txc2 = str2 != NULL;

	if(tx1 && tx2) {
		return 0;
	} else if(tx1) {
		return -1;
	} else if(tx2) {
		return 1;
	} else if(txc1 && txc2) {
		return 0;
	} else if(txc1) {
		return -1;
	} else if(txc2) {
		return 1;
	} else {
		return 0;
	}
}

static gint do_sort(GtkFlowBoxChild* child1, GtkFlowBoxChild* child2, gpointer data) {
	(void) data;
	gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(inner_box), 0);

	GtkWidget* box1 = gtk_bin_get_child(GTK_BIN(child1));
	GtkWidget* box2 = gtk_bin_get_child(GTK_BIN(child2));
	if(GTK_IS_EXPANDER(box1)) {
		box1 = gtk_expander_get_label_widget(GTK_EXPANDER(box1));
	}
	if(GTK_IS_EXPANDER(box2)) {
		box2 = gtk_expander_get_label_widget(GTK_EXPANDER(box2));
	}

	const gchar* text1 = wofi_property_box_get_property(WOFI_PROPERTY_BOX(box1), "filter");
	const gchar* text2 = wofi_property_box_get_property(WOFI_PROPERTY_BOX(box2), "filter");
	uint64_t index1 = strtol(wofi_property_box_get_property(WOFI_PROPERTY_BOX(box1), "index"), NULL, 10);
	uint64_t index2 = strtol(wofi_property_box_get_property(WOFI_PROPERTY_BOX(box2), "index"), NULL, 10);

	if(text1 == NULL || text2 == NULL) {
		return index1 - index2;
	}

	uint64_t fallback = 0;
	switch(sort_order) {
	case SORT_ORDER_DEFAULT:
		fallback = index1 - index2;
		break;
	case SORT_ORDER_ALPHABETICAL:
		if(insensitive) {
			fallback = strcasecmp(text1, text2);
		} else {
			fallback = strcmp(text1, text2);
		}
		break;
	}

	if(filter == NULL || strcmp(filter, "") == 0) {
		return fallback;
	}

	gint primary = 0;
	switch(matching) {
	case MATCHING_MODE_CONTAINS:
		primary = contains_sort(text1, text2);
		if(primary == 0) {
			return fallback;
		}
		return primary;
	case MATCHING_MODE_FUZZY:
		primary = fuzzy_sort(text1, text2);
		if(primary == 0) {
			return fallback;
		}
		return primary;
	default:
		return 0;
	}
}

static void select_first(void) {
	GtkFlowBoxChild* child = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(inner_box), 1);
	gtk_widget_grab_focus(GTK_WIDGET(child));
	gtk_flow_box_select_child(GTK_FLOW_BOX(inner_box), GTK_FLOW_BOX_CHILD(child));
}

static GdkModifierType get_mask_from_keyval(guint keyval) {
	switch(keyval) {
	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
		return GDK_SHIFT_MASK;
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
		return GDK_CONTROL_MASK;
	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
		return GDK_MOD1_MASK;
	default:
		return 0;
	}
}

static GdkModifierType get_mask_from_name(char* name) {
	return get_mask_from_keyval(gdk_keyval_from_name(name));
}

static void move_up(void) {
	user_moved = true;
	gtk_widget_child_focus(window, GTK_DIR_UP);
}

static void move_down(void) {
	user_moved = true;
	if(outer_orientation == GTK_ORIENTATION_VERTICAL) {
		if(gtk_widget_has_focus(entry) || gtk_widget_has_focus(scroll)) {
			select_first();
			return;
		}
	}
	gtk_widget_child_focus(window, GTK_DIR_DOWN);
}

static void move_left(void) {
	user_moved = true;
	gtk_widget_child_focus(window, GTK_DIR_LEFT);
}

static void move_right(void) {
	user_moved = true;
	if(outer_orientation == GTK_ORIENTATION_HORIZONTAL) {
		if(gtk_widget_has_focus(entry) || gtk_widget_has_focus(scroll)) {
			select_first();
			return;
		}
	}
	gtk_widget_child_focus(window, GTK_DIR_RIGHT);
}

static void move_forward(void) {
	user_moved = true;
	if(gtk_widget_has_focus(entry) || gtk_widget_has_focus(scroll)) {
		select_first();
		return;
	}
	gtk_widget_child_focus(window, GTK_DIR_TAB_FORWARD);
}

static void move_backward(void) {
	user_moved = true;
	gtk_widget_child_focus(window, GTK_DIR_TAB_BACKWARD);
}

static void move_pgup(void) {
	uint64_t lines = height / max_height;
	for(size_t count = 0; count < lines; ++count) {
		move_up();
	}
}

static void move_pgdn(void) {
	uint64_t lines = height / max_height;
	for(size_t count = 0; count < lines; ++count) {
		move_down();
	}
}

static void do_exit(void) {
	exit(1);
}

static void do_expand(void) {
	GList* children = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(inner_box));
	if(children->data != NULL && gtk_widget_has_focus(children->data)) {
		GtkWidget* expander = gtk_bin_get_child(children->data);
		if(GTK_IS_EXPANDER(expander)) {
			g_signal_emit_by_name(expander, "activate");
		}
	}
	g_list_free(children);
}

static void do_hide_search(void) {
	gtk_widget_set_visible(entry, !gtk_widget_get_visible(entry));
	update_surface_size();
}

static void do_copy(void) {
	GList* children = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(inner_box));
	if(children->data != NULL && gtk_widget_has_focus(children->data)) {
		GtkWidget* widget = gtk_bin_get_child(children->data);
		if(GTK_IS_EXPANDER(widget)) {
			GtkWidget* box = gtk_bin_get_child(GTK_BIN(widget));
			GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(box));
			if(row == NULL) {
				widget = gtk_expander_get_label_widget(GTK_EXPANDER(widget));
			} else {
				widget = gtk_bin_get_child(GTK_BIN(row));
			}
		}
		if(WOFI_IS_PROPERTY_BOX(widget)) {
			const gchar* action = wofi_property_box_get_property(WOFI_PROPERTY_BOX(widget), "action");
			if(action == NULL) {
				return;
			}

			int fds[2];
			pipe(fds);
			if(fork() == 0) {
				close(fds[1]);
				dup2(fds[0], STDIN_FILENO);
				execlp(copy_exec, copy_exec, NULL);
				fprintf(stderr, "%s could not be executed: %s\n", copy_exec, strerror(errno));
				exit(errno);
			}
			close(fds[0]);

			write(fds[1], action, strlen(action));

			close(fds[1]);

			while(waitpid(-1, NULL, WNOHANG) > 0);
		}
	}
}

static bool do_key_action(GdkEvent* event, char* mod, void (*action)(void)) {
	if(mod != NULL) {
		GdkModifierType mask = get_mask_from_name(mod);
		if((event->key.state & mask) == mask) {
			event->key.state &= ~mask;
			action();
			return true;
		}
		return false;
	} else {
		action();
		return true;
	}
}

static bool has_mod(guint state) {
	return (state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK || (state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK || (state & GDK_MOD1_MASK) == GDK_MOD1_MASK;
}

static gboolean do_nyan(gpointer data) {
	(void) data;
	wofi_load_css(true);
	return G_SOURCE_CONTINUE;
}

static guint get_konami_key(uint8_t cycle) {
	switch(cycle) {
	case 0:
		return GDK_KEY_Up;
	case 1:
		return GDK_KEY_Up;
	case 2:
		return GDK_KEY_Down;
	case 3:
		return GDK_KEY_Down;
	case 4:
		return GDK_KEY_Left;
	case 5:
		return GDK_KEY_Right;
	case 6:
		return GDK_KEY_Left;
	case 7:
		return GDK_KEY_Right;
	case 8:
		return GDK_KEY_b;
	case 9:
		return GDK_KEY_a;
	case 10:
		return GDK_KEY_Return;
	default:
		return GDK_KEY_VoidSymbol;
	}
}

static gboolean key_press(GtkWidget* widget, GdkEvent* event, gpointer data) {
	(void) widget;
	(void) data;
	gchar* name = gdk_keyval_name(event->key.keyval);
	bool printable = strlen(name) == 1 && isprint(name[0]) && !has_mod(event->key.state);

	guint konami_key = get_konami_key(konami_cycle);
	if(event->key.keyval == konami_key) {
		if(konami_cycle == 10) {
			konami_cycle = 0;
			if(!is_konami) {
				is_konami = true;
				gdk_threads_add_timeout(500, do_nyan, NULL);
			}
			return TRUE;
		} else {
			++konami_cycle;
		}
	} else {
		konami_cycle = 0;
	}

	if(gtk_widget_has_focus(entry) && printable) {
		return FALSE;
	}


	bool key_success = true;
	struct key_entry* key_ent = map_get(keys, gdk_keyval_name(event->key.keyval));

	if(key_ent != NULL && key_ent->action != NULL) {
		key_success = do_key_action(event, key_ent->mod, key_ent->action);
	} else if(key_ent != NULL) {
		mod_shift = (event->key.state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;
		mod_ctrl = (event->key.state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
		if(mod_shift) {
			event->key.state &= ~GDK_SHIFT_MASK;
		}
		if(mod_ctrl) {
			event->key.state &= ~GDK_CONTROL_MASK;
		}
		if(gtk_widget_has_focus(scroll)) {
			gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entry));
		}
		GList* children = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(inner_box));
		if(gtk_widget_has_focus(entry)) {
			g_signal_emit_by_name(entry, "activate", entry, NULL);
		} else if(gtk_widget_has_focus(inner_box) || children->data != NULL) {
			gpointer obj = children->data;

			if(obj != NULL) {
				GtkWidget* exp = gtk_bin_get_child(GTK_BIN(obj));
				if(GTK_IS_EXPANDER(exp)) {
					GtkWidget* box = gtk_bin_get_child(GTK_BIN(exp));
					GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(box));
					if(row != NULL) {
						obj = row;
					}
				}
				g_signal_emit_by_name(obj, "activate", obj, NULL);
			}
		}
		g_list_free(children);
	} else if(event->key.keyval == GDK_KEY_Shift_L || event->key.keyval == GDK_KEY_Shift_R) {
	} else if(event->key.keyval == GDK_KEY_Control_L || event->key.keyval == GDK_KEY_Control_R) {
	} else if(event->key.keyval == GDK_KEY_Alt_L || event->key.keyval == GDK_KEY_Alt_R) {
	} else {
		key_success = false;
	}

	if(key_success) {
		return TRUE;
	}
	if(!gtk_widget_has_focus(entry)) {
		gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entry));
	}
	return FALSE;
}

static gboolean focus(GtkWidget* widget, GdkEvent* event, gpointer data) {
	(void) data;
	if(event->focus_change.in) {
		gtk_widget_set_state_flags(widget, GTK_STATE_FLAG_FOCUSED, TRUE);
	} else {
		gtk_widget_set_state_flags(widget, GTK_STATE_FLAG_NORMAL, TRUE);
	}
	return FALSE;
}

static gboolean focus_entry(GtkWidget* widget, GdkEvent* event, gpointer data) {
	(void) data;
	if(widget == entry && dbus != NULL) {
		GError* err = NULL;
		GVariant* ret = g_dbus_proxy_call_sync(dbus, "SetVisible", g_variant_new("(b)", event->focus_change.in), G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &err);
		g_variant_unref(ret);
		if(err != NULL) {
			if(err->code != G_DBUS_ERROR_SERVICE_UNKNOWN) {
				fprintf(stderr, "Error while changing OSK state %s\n", err->message);
			}
			g_error_free(err);
		}
	}
	return FALSE;
}

static void* get_plugin_proc(const char* prefix, const char* suffix) {
	char* proc_name = utils_concat(3, "wofi_", prefix, suffix);
	void* proc = dlsym(RTLD_DEFAULT, proc_name);
	free(proc_name);
	return proc;
}

static void* load_mode(char* _mode, char* name, struct mode* mode_ptr, struct map* props) {
	char* dso = strstr(_mode, ".so");

	mode_ptr->name = strdup(name);

	void (*init)(struct mode* _mode, struct map* props);
	void (*load)(struct mode* _mode);
	const char** (*get_arg_names)(void);
	size_t (*get_arg_count)(void);
	bool (*no_entry)(void);
	if(dso == NULL) {
		mode_ptr->dso = NULL;
		init = get_plugin_proc(_mode, "_init");
		load = get_plugin_proc(_mode, "_load");
		get_arg_names = get_plugin_proc(_mode, "_get_arg_names");
		get_arg_count = get_plugin_proc(_mode, "_get_arg_count");
		mode_ptr->mode_exec = get_plugin_proc(_mode, "_exec");
		mode_ptr->mode_get_widget = get_plugin_proc(_mode, "_get_widget");
		no_entry = get_plugin_proc(_mode, "_no_entry");
	} else {
		char* plugins_dir = utils_concat(2, config_dir, "/plugins/");
		char* full_name = utils_concat(2, plugins_dir, _mode);
		mode_ptr->dso = strdup(full_name);
		void* plugin = dlopen(full_name, RTLD_LAZY | RTLD_LOCAL);
		free(full_name);
		free(plugins_dir);
		init = dlsym(plugin, "init");
		load = dlsym(plugin, "load");
		get_arg_names = dlsym(plugin, "get_arg_names");
		get_arg_count = dlsym(plugin, "get_arg_count");
		mode_ptr->mode_exec = dlsym(plugin, "exec");
		mode_ptr->mode_get_widget = dlsym(plugin, "get_widget");
		no_entry = dlsym(plugin, "no_entry");
	}

	if(load != NULL) {
		load(mode_ptr);
	}

	const char** arg_names = NULL;
	size_t arg_count = 0;
	if(get_arg_names != NULL && get_arg_count != NULL) {
		arg_names = get_arg_names();
		arg_count = get_arg_count();
	}

	if(mode == NULL && no_entry != NULL && no_entry()) {
		mode = mode_ptr->name;
	}

	for(size_t count = 0; count < arg_count; ++count) {
		const char* arg = arg_names[count];
		char* full_name = utils_concat(3, name, "-", arg);
		map_put(props, arg, config_get(config, full_name, NULL));
		free(full_name);
	}
	return init;
}

static struct mode* add_mode(char* _mode) {
	struct mode* mode_ptr = calloc(1, sizeof(struct mode));
	struct map* props = map_init();
	void (*init)(struct mode* _mode, struct map* props) = load_mode(_mode, _mode, mode_ptr, props);

	if(init == NULL) {
		free(mode_ptr->name);
		free(mode_ptr->dso);
		free(mode_ptr);
		map_free(props);

		mode_ptr = calloc(1, sizeof(struct mode));
		props = map_init();

		char* name = utils_concat(3, "lib", _mode, ".so");
		init = load_mode(name, _mode, mode_ptr, props);
		free(name);

		if(init == NULL) {
			free(mode_ptr->name);
			free(mode_ptr->dso);
			free(mode_ptr);
			map_free(props);

			mode_ptr = calloc(1, sizeof(struct mode));
			props = map_init();

			init = load_mode("external", _mode, mode_ptr, props);

			map_put(props, "exec", _mode);

			if(init == NULL) {
				fprintf(stderr, "I would love to show %s but Idk what it is\n", _mode);
				exit(1);
			}
		}
	}
	map_put_void(modes, _mode, mode_ptr);
	init(mode_ptr, props);

	map_free(props);
	return mode_ptr;
}

static void* start_mode_thread(void* data) {
	char* mode = data;
	if(strchr(mode, ',') != NULL) {
		char* save_ptr;
		char* str = strtok_r(mode, ",", &save_ptr);
		do {
			struct mode* mode_ptr = add_mode(str);
			wl_list_insert(&mode_list, &mode_ptr->link);
		} while((str = strtok_r(NULL, ",", &save_ptr)) != NULL);
	} else {
		struct mode* mode_ptr = add_mode(mode);
		wl_list_insert(&mode_list, &mode_ptr->link);
	}
	return NULL;
}

static void parse_mods(char* key, void (*action)(void)) {
	char* tmp = strdup(key);
	char* save_ptr;
	char* str = strtok_r(tmp, ",", &save_ptr);
	do {
		if(str == NULL) {
			break;
		}
		char* hyphen = strchr(str, '-');
		char* mod;
		if(hyphen != NULL) {
			*hyphen = 0;
			guint key1 = gdk_keyval_from_name(str);
			guint key2 = gdk_keyval_from_name(hyphen + 1);
			if(get_mask_from_keyval(key1) != 0) {
				mod = str;
				str = hyphen + 1;
			} else if(get_mask_from_keyval(key2) != 0) {
				mod = hyphen + 1;
			} else {
				fprintf(stderr, "Neither %s nor %s is a modifier, this is not supported\n", str, hyphen + 1);
				mod = NULL;
			}
		} else {
			mod = NULL;
		}
		struct key_entry* entry = malloc(sizeof(struct key_entry));
		if(mod == NULL) {
			entry->mod = NULL;
		} else {
			entry->mod = strdup(mod);
		}
		entry->action = action;
		map_put_void(keys, str, entry);
	} while((str = strtok_r(NULL, ",", &save_ptr)) != NULL);
	free(tmp);
}

static void get_output_name(void* data, struct zxdg_output_v1* output, const char* name) {
	(void) output;
	struct output_node* node = data;
	node->name = strdup(name);
}

static void get_output_res(void* data, struct zxdg_output_v1* output, int32_t width, int32_t height) {
	(void) output;
	struct output_node* node = data;
	node->width = width;
	node->height = height;
}

static void get_output_pos(void* data, struct zxdg_output_v1* output, int32_t x, int32_t y) {
	(void) output;
	struct output_node* node = data;
	node->x = x;
	node->y = y;
}

static gboolean do_percent_size(gpointer data) {
	char** geo_str = data;
	bool width_percent = strchr(geo_str[0], '%') != NULL;
	bool height_percent = strchr(geo_str[1], '%') != NULL && lines == 0;

	GdkMonitor* monitor = gdk_display_get_monitor_at_window(gdk_display_get_default(), gtk_widget_get_window(window));

	GdkRectangle rect;
	gdk_monitor_get_geometry(monitor, &rect);

	if(rect.width == resolution.width && rect.height == resolution.height) {
		return G_SOURCE_CONTINUE;
	}

	resolution = rect;

	if(width_percent) {
		uint64_t w_percent = strtol(geo_str[0], NULL, 10);
		width = (w_percent / 100.f) * rect.width;
	}
	if(height_percent) {
		uint64_t h_percent = strtol(geo_str[1], NULL, 10);
		height = (h_percent / 100.f) * rect.height;
	}
	update_surface_size();
	return G_SOURCE_CONTINUE;
}

void wofi_init(struct map* _config) {
	config = _config;
	char* width_str = config_get(config, "width", "50%");
	char* height_str = config_get(config, "height", "40%");
	width = strtol(width_str, NULL, 10);
	height = strtol(height_str, NULL, 10);
	x = map_get(config, "x");
	y = map_get(config, "y");
	bool normal_window = strcmp(config_get(config, "normal_window", "false"), "true") == 0;
	char* mode = map_get(config, "mode");
	GtkOrientation orientation = config_get_mnemonic(config, "orientation", "vertical", 2, "vertical", "horizontal");
	outer_orientation = config_get_mnemonic(config, "orientation", "vertical", 2, "horizontal", "vertical");
	GtkAlign halign = config_get_mnemonic(config, "halign", "fill", 4, "fill", "start", "end", "center");
	content_halign = config_get_mnemonic(config, "content_halign", "fill", 4, "fill", "start", "end", "center");
	char* default_valign = "start";
	if(outer_orientation == GTK_ORIENTATION_HORIZONTAL) {
		default_valign = "center";
	}
	GtkAlign valign = config_get_mnemonic(config, "valign", default_valign, 4, "fill", "start", "end", "center");
	char* prompt = config_get(config, "prompt", mode);
	uint64_t filter_rate = strtol(config_get(config, "filter_rate", "100"), NULL, 10);
	allow_images = strcmp(config_get(config, "allow_images", "false"), "true") == 0;
	allow_markup = strcmp(config_get(config, "allow_markup", "false"), "true") == 0;
	image_size = strtol(config_get(config, "image_size", "32"), NULL, 10);
	cache_file = map_get(config, "cache_file");
	config_dir = map_get(config, "config_dir");
	terminal = map_get(config, "term");
	char* password_char = map_get(config, "password_char");
	exec_search = strcmp(config_get(config, "exec_search", "false"), "true") == 0;
	bool hide_scroll = strcmp(config_get(config, "hide_scroll", "false"), "true") == 0;
	matching = config_get_mnemonic(config, "matching", "contains", 2, "contains", "fuzzy");
	insensitive = strcmp(config_get(config, "insensitive", "false"), "true") == 0;
	parse_search = strcmp(config_get(config, "parse_search", "false"), "true") == 0;
	location = config_get_mnemonic(config, "location", "center", 18,
			"center", "top_left", "top", "top_right", "right", "bottom_right", "bottom", "bottom_left", "left",
			"0", "1", "2", "3", "4", "5", "6", "7", "8");
	no_actions = strcmp(config_get(config, "no_actions", "false"), "true") == 0;
	lines = strtol(config_get(config, "lines", "0"), NULL, 10);
	max_lines = lines;
	columns = strtol(config_get(config, "columns", "1"), NULL, 10);
	sort_order = config_get_mnemonic(config, "sort_order", "default", 2, "default", "alphabetical");
	line_wrap = config_get_mnemonic(config, "line_wrap", "off", 4, "off", "word", "char", "word_char") - 1;
	bool global_coords = strcmp(config_get(config, "global_coords", "false"), "true") == 0;
	bool hide_search = strcmp(config_get(config, "hide_search", "false"), "true") == 0;
	char* search = map_get(config, "search");
	dynamic_lines = strcmp(config_get(config, "dynamic_lines", "false"), "true") == 0;
	char* monitor = map_get(config, "monitor");
	char* layer = config_get(config, "layer", "top");
	copy_exec = config_get(config, "copy_exec", "wl-copy");

	keys = map_init_void();



	char* key_up = config_get(config, "key_up", "Up");
	char* key_down = config_get(config, "key_down", "Down");
	char* key_left = config_get(config, "key_left", "Left");
	char* key_right = config_get(config, "key_right", "Right");
	char* key_forward = config_get(config, "key_forward", "Tab");
	char* key_backward = config_get(config, "key_backward", "ISO_Left_Tab");
	char* key_submit = config_get(config, "key_submit", "Return");
	char* key_exit = config_get(config, "key_exit", "Escape");
	char* key_pgup = config_get(config, "key_pgup", "Page_Up");
	char* key_pgdn = config_get(config, "key_pgdn", "Page_Down");
	char* key_expand = config_get(config, "key_expand", "");
	char* key_hide_search = config_get(config, "key_hide_search", "");
	char* key_copy = config_get(config, "key_copy", "Control_L-c");

	parse_mods(key_up, move_up);
	parse_mods(key_down, move_down);
	parse_mods(key_left, move_left);
	parse_mods(key_right, move_right);
	parse_mods(key_forward, move_forward);
	parse_mods(key_backward, move_backward);
	parse_mods(key_submit, NULL); //submit is a special case, when a NULL action is encountered submit is used instead
	parse_mods(key_exit, do_exit);
	parse_mods(key_pgup, move_pgup);
	parse_mods(key_pgdn, move_pgdn);
	parse_mods(key_expand, do_expand);
	parse_mods(key_hide_search, do_hide_search);
	parse_mods(key_copy, do_copy);

	modes = map_init_void();

	if(lines > 0) {
		height = 1;
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(window);
	gtk_widget_set_name(window, "window");
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	gtk_window_resize(GTK_WINDOW(window), width, height);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

	if(!normal_window) {
		wl_list_init(&outputs);
		wl = gdk_wayland_display_get_wl_display(gdk_display_get_default());

		if(wl == NULL) {
			fprintf(stderr, "Failed to connect to wayland compositor\n");
			exit(1);
		}

		struct wl_registry* registry = wl_display_get_registry(wl);
		struct wl_registry_listener listener = {
			.global = add_interface,
			.global_remove = nop
		};
		wl_registry_add_listener(registry, &listener, NULL);
		wl_display_roundtrip(wl);

		if(shell == NULL) {
			fprintf(stderr, "Compositor does not support wlr_layer_shell protocol, switching to normal window mode\n");
			normal_window = true;
			goto normal_win;
		}

		struct output_node* node;
		wl_list_for_each(node, &outputs, link) {
			struct zxdg_output_v1* xdg_output = zxdg_output_manager_v1_get_xdg_output(output_manager, node->output);
			struct zxdg_output_v1_listener* xdg_listener = malloc(sizeof(struct zxdg_output_v1_listener));
			xdg_listener->description = nop;
			xdg_listener->done = nop;
			xdg_listener->logical_position = get_output_pos;
			xdg_listener->logical_size = get_output_res;
			xdg_listener->name = get_output_name;
			zxdg_output_v1_add_listener(xdg_output, xdg_listener, node);
		}
		wl_display_roundtrip(wl);

		ix = x == NULL ? 0 : strtol(x, NULL, 10);
		iy = y == NULL ? 0 : strtol(y, NULL, 10);

		struct wl_output* output = NULL;
		if(global_coords) {
			wl_list_for_each(node, &outputs, link) {
				if(ix >= node->x && ix <= node->width + node->x
						&& iy >= node->y && iy <= node->height + node->y) {
					output = node->output;
					ix -= node->x;
					iy -= node->y;
					break;
				}
			}
		} else if(monitor != NULL) {
			wl_list_for_each(node, &outputs, link) {
				if(strcmp(monitor, node->name) == 0) {
					output = node->output;
					break;
				}
			}
		}

		GdkWindow* gdk_win = gtk_widget_get_window(window);

		gdk_wayland_window_set_use_custom_surface(gdk_win);
		wl_surface = gdk_wayland_window_get_wl_surface(gdk_win);

		enum zwlr_layer_shell_v1_layer wlr_layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;

		if(strcmp(layer, "background") == 0) {
			wlr_layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
		} else if(strcmp(layer, "bottom") == 0) {
			wlr_layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
		} else if(strcmp(layer, "top") == 0) {
			wlr_layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
		} else if(strcmp(layer, "overlay") == 0) {
			wlr_layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
		}

		wlr_surface = zwlr_layer_shell_v1_get_layer_surface(shell, wl_surface, output, wlr_layer, "wofi");
		setup_surface(wlr_surface);
		struct zwlr_layer_surface_v1_listener* surface_listener = malloc(sizeof(struct zwlr_layer_surface_v1_listener));
		surface_listener->configure = config_surface;
		surface_listener->closed = nop;
		zwlr_layer_surface_v1_add_listener(wlr_surface, surface_listener, NULL);
		wl_surface_commit(wl_surface);
		wl_display_roundtrip(wl);
	}

	normal_win:

	outer_box = gtk_box_new(outer_orientation, 0);
	gtk_widget_set_name(outer_box, "outer-box");
	gtk_container_add(GTK_CONTAINER(window), outer_box);
	entry = gtk_search_entry_new();

	gtk_widget_set_name(entry, "input");
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), prompt);
	gtk_container_add(GTK_CONTAINER(outer_box), entry);
	gtk_widget_set_child_visible(entry, !hide_search);

	if(search != NULL) {
		gtk_entry_set_text(GTK_ENTRY(entry), search);
	}

	if(password_char != NULL) {
		gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
		gtk_entry_set_invisible_char(GTK_ENTRY(entry), password_char[0]);
	}

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(scroll, "scroll");
	gtk_container_add(GTK_CONTAINER(outer_box), scroll);
	gtk_widget_set_size_request(scroll, width, height);
	if(hide_scroll) {
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL);
	}

	inner_box = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(inner_box), GTK_SELECTION_BROWSE);
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(inner_box), columns);
	gtk_orientable_set_orientation(GTK_ORIENTABLE(inner_box), orientation);
	gtk_widget_set_halign(inner_box, halign);
	gtk_widget_set_valign(inner_box, valign);

	gtk_widget_set_name(inner_box, "inner-box");
	gtk_flow_box_set_activate_on_single_click(GTK_FLOW_BOX(inner_box), FALSE);

	GtkWidget* wrapper_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(wrapper_box), TRUE);
	gtk_container_add(GTK_CONTAINER(wrapper_box), inner_box);
	gtk_container_add(GTK_CONTAINER(scroll), wrapper_box);

	switch(matching) {
	case MATCHING_MODE_CONTAINS:
		gtk_flow_box_set_filter_func(GTK_FLOW_BOX(inner_box), do_filter, NULL, NULL);
		gtk_flow_box_set_sort_func(GTK_FLOW_BOX(inner_box), do_sort, NULL, NULL);
		break;
	case MATCHING_MODE_FUZZY:
		gtk_flow_box_set_sort_func(GTK_FLOW_BOX(inner_box), do_sort, NULL, NULL);
		break;
	}

	g_signal_connect(inner_box, "child-activated", G_CALLBACK(activate_item), NULL);
	g_signal_connect(inner_box, "selected-children-changed", G_CALLBACK(select_item), NULL);
	g_signal_connect(entry, "activate", G_CALLBACK(activate_search), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(key_press), NULL);
	g_signal_connect(window, "focus-in-event", G_CALLBACK(focus), NULL);
	g_signal_connect(window, "focus-out-event", G_CALLBACK(focus), NULL);
	g_signal_connect(entry, "focus-in-event", G_CALLBACK(focus_entry), NULL);
	g_signal_connect(entry, "focus-out-event", G_CALLBACK(focus_entry), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(do_exit), NULL);

	dbus = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL, "sm.puri.OSK0", "/sm/puri/OSK0", "sm.puri.OSK0", NULL, NULL);

	gdk_threads_add_timeout(filter_rate, do_search, NULL);


	bool width_percent = strchr(width_str, '%') != NULL;
	bool height_percent = strchr(height_str, '%') != NULL && lines == 0;
	if(width_percent || height_percent) {
		static char* geo_str[2];
		geo_str[0] = width_str;
		geo_str[1] = height_str;
		gdk_threads_add_timeout(50, do_percent_size, geo_str);
		do_percent_size(geo_str);
	}

	wl_list_init(&mode_list);

	pthread_create(&mode_thread, NULL, start_mode_thread, mode);

	gdk_threads_add_idle(insert_all_widgets, &mode_list);

	gtk_window_set_title(GTK_WINDOW(window), prompt);
	gtk_widget_show_all(window);
}
