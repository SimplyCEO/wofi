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

#include <map.h>
#include <wofi.h>
#include <utils.h>
#include <config.h>

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>

static char* CONFIG_LOCATION;
static char* COLORS_LOCATION;
static struct map* config;
static char* config_path;
static char* stylesheet;
static char* color_path;

static void print_usage(char** argv) {
	char* slash = strrchr(argv[0], '/');
	uint64_t offset;
	if(slash == NULL) {
		offset = 0;
	} else {
		offset = (slash - argv[0]) + 1;
	}
	printf("%s [options]\n", argv[0] + offset);
	printf("Options:\n");
	printf("--help\t\t-h\tDisplays this help message\n");
	printf("--fork\t\t-f\tForks the menu so you can close the terminal\n");
	printf("--conf\t\t-c\tSelects a config file to use\n");
	printf("--style\t\t-s\tSelects a stylesheet to use\n");
	printf("--color\t\t-C\tSelects a colors file to use\n");
	printf("--dmenu\t\t-d\tRuns in dmenu mode\n");
	printf("--show\t\t-S\tSpecifies the mode to run in\n");
	printf("--width\t\t-W\tSpecifies the surface width\n");
	printf("--height\t-H\tSpecifies the surface height\n");
	printf("\t\t-p\tPrompt to display\n");
	printf("--xoffset\t-x\tThe x offset\n");
	printf("--yoffset\t-y\tThe y offset\n");
	exit(0);
}

static void load_css() {
	if(access(stylesheet, R_OK) == 0) {
		FILE* file = fopen(stylesheet, "r");
		fseek(file, 0, SEEK_END);
		ssize_t size = ftell(file);
		fseek(file, 0, SEEK_SET);
		char* data = malloc(size + 1);
		fread(data, 1, size, file);
		fclose(file);

		data[size] = 0;
		struct wl_list lines;
		struct node {
			char* line;
			struct wl_list link;
		};
		wl_list_init(&lines);
		if(access(color_path, R_OK) == 0) {
			file = fopen(color_path, "r");
			char* line = NULL;
			size_t line_size = 0;
			ssize_t line_l = 0;
			while((line_l = getline(&line, &line_size, file)) != -1) {
				struct node* entry = malloc(sizeof(struct node));
				line[line_l - 1] = 0;
				entry->line = malloc(line_l + 1);
				strcpy(entry->line, line);
				wl_list_insert(&lines, &entry->link);
			}
			fclose(file);
			free(line);
		}

		ssize_t count = wl_list_length(&lines) - 1;
		if(count > 99) {
			fprintf(stderr, "Woah there that's a lot of colors. Try having no more than 99, thanks\n");
			exit(1);
		}
		struct node* node;
		wl_list_for_each(node, &lines, link) {
			//Do --wofi-color replace
			const char* color = node->line;
			const char* wofi_color = "--wofi-color";
			char count_str[3];
			snprintf(count_str, 3, "%lu", count--);
			char* needle = utils_concat(2, wofi_color, count_str);
			size_t color_len = strlen(color);
			size_t needle_len = strlen(needle);
			if(color_len > needle_len) {
				free(needle);
				fprintf(stderr, "What color format is this, try #FFFFFF, kthxbi\n");
				continue;
			}
			char* replace = strstr(data, needle);
			while(replace != NULL) {
				memcpy(replace, color, color_len);
				memset(replace + color_len, ' ', needle_len - color_len);
				replace = strstr(data, needle);
			}
			free(needle);


			//Do --wofi-rgb-color replace
			if(color_len < 7) {
				fprintf(stderr, "What color format is this, try #FFFFFF, kthxbi\n");
				continue;
			}
			const char* wofi_rgb_color = "--wofi-rgb-color";
			needle = utils_concat(2, wofi_rgb_color, count_str);
			needle_len = strlen(needle);
			replace = strstr(data, needle);
			while(replace != NULL) {
				char r[3];
				char g[3];
				char b[3];
				memcpy(r, color + 1, 2);
				memcpy(g, color + 3, 2);
				memcpy(b, color + 5, 2);
				r[2] = 0;
				g[2] = 0;
				b[2] = 0;
				char rgb[14];
				snprintf(rgb, 14, "%ld, %ld, %ld", strtol(r, NULL, 16), strtol(g, NULL, 16), strtol(b, NULL, 16));
				size_t rgb_len = strlen(rgb);
				memcpy(replace, rgb, rgb_len);
				memset(replace + rgb_len, ' ', needle_len - rgb_len);
				replace = strstr(data, needle);
			}
			free(needle);
		}
		GtkCssProvider* css = gtk_css_provider_new();
		gtk_css_provider_load_from_data(css, data, strlen(data), NULL);
		free(data);
		struct node* tmp;
		wl_list_for_each_safe(node, tmp, &lines, link) {
			free(node->line);
			wl_list_remove(&node->link);
			free(node);
		}
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
}

int main(int argc, char** argv) {

	const struct option opts[] = {
		{
			.name = "help",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'h'
		},
		{
			.name = "fork",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'f'
		},
		{
			.name = "conf",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'c'
		},
		{
			.name = "style",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 's'
		},
		{
			.name = "color",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'C'
		},
		{
			.name = "dmenu",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'd'
		},
		{
			.name = "show",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'S'
		},
		{
			.name = "width",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'W'
		},
		{
			.name = "height",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'H'
		},
		{
			.name = "xoffset",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'x'
		},
		{
			.name = "yoffset",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'y'
		},
		{
			.name = NULL,
			.has_arg = 0,
			.flag = NULL,
			.val = 0
		}
	};

	const char* config_str = NULL;
	char* style_str = NULL;
	char* color_str = NULL;
	char* mode = NULL;
	char* prompt = NULL;
	char* width = NULL;
	char* height = NULL;
	char* x = NULL;
	char* y = NULL;
	char opt;
	while((opt = getopt_long(argc, argv, "hfc:s:C:dS:W:H:p:x:y:", opts, NULL)) != -1) {
		switch(opt) {
		case 'h':
			print_usage(argv);
			break;
		case 'f':
			if(fork() > 0) {
				exit(0);
			}
			fclose(stdout);
			fclose(stderr);
			fclose(stdin);
			break;
		case 'c':
			config_str = optarg;
			break;
		case 's':
			style_str = optarg;
			break;
		case 'C':
			color_str = optarg;
			break;
		case 'd':
			mode = "dmenu";
			break;
		case 'S':
			mode = optarg;
			break;
		case 'W':
			width = optarg;
			break;
		case 'H':
			height = optarg;
			break;
		case 'p':
			prompt = optarg;
			break;
		case 'x':
			x = optarg;
			break;
		case 'y':
			y = optarg;
			break;
		}
	}

	const char* home_dir = getenv("HOME");
	const char* xdg_conf = getenv("XDG_CONFIG_HOME");
	if(xdg_conf == NULL) {
		CONFIG_LOCATION = utils_concat(2, home_dir, "/.config/wofi");
	} else {
		CONFIG_LOCATION = utils_concat(2, xdg_conf, "/wofi");
	}

	const char* xdg_cache = getenv("XDG_CACHE_HOME");
	if(xdg_cache == NULL) {
		COLORS_LOCATION = utils_concat(2, home_dir, "/.cache/wal/colors");
	} else {
		COLORS_LOCATION = utils_concat(2, xdg_cache, "/wal/colors");
	}

	config = map_init();

	//Check if --conf was specified
	if(config_str == NULL) {
		const char* config_f = "/config";
		config_path = utils_concat(2, CONFIG_LOCATION, config_f);
	} else {
		config_path = strdup(config_str);
	}
	if(access(config_path, R_OK) == 0) {
		config_load(config, config_path);
	}
	free(config_path);

	//Check if --style was specified
	if(style_str == NULL) {
		style_str = map_get(config, "stylesheet");
		if(style_str == NULL) {
			const char* style_f = "/style.css";
			stylesheet = utils_concat(2, CONFIG_LOCATION, style_f);
		} else {
			if(style_str[0] == '/') {
				stylesheet = strdup(style_str);
			} else {
				stylesheet = utils_concat(3, CONFIG_LOCATION, "/", style_str);
			}
		}
	} else {
		stylesheet = strdup(style_str);
	}

	//Check if --color was specified
	if(color_str == NULL) {
		color_str = map_get(config, "colors");
		if(color_str == NULL) {
			color_path = strdup(COLORS_LOCATION);
		} else {
			if(color_str[0] == '/') {
				color_path = strdup(color_str);
			} else {
				color_path = utils_concat(3, CONFIG_LOCATION, "/", color_str);
			}
		}
	} else {
		color_path = strdup(color_str);
	}

	free(COLORS_LOCATION);

	if(mode != NULL) {
		map_put(config, "mode", mode);
	} else if(map_get(config, "mode") == NULL) {
		fprintf(stderr, "I need a mode, please give me a mode, that's what --show is for\n");
		exit(1);
	}
	if(width != NULL) {
		map_put(config, "width", width);
	}
	if(height != NULL) {
		map_put(config, "height", height);
	}
	if(prompt != NULL) {
		map_put(config, "prompt", prompt);
	}
	if(x != NULL) {
		map_put(config, "x", x);
	}
	if(y != NULL) {
		map_put(config, "y", y);
	}

	gtk_init(&argc, &argv);

	load_css();

	wofi_init(config);
	gtk_main();
	return 0;
}
