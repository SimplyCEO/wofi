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
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <map.h>
#include <wofi.h>
#include <utils.h>
#include <config.h>

#include <wayland-client.h>

#include <gtk/gtk.h>

static const char* nyan_colors[] = {"#FF0000", "#FFA500", "#FFFF00", "#00FF00", "#0000FF", "#FF00FF"};
static size_t nyan_color_l = sizeof(nyan_colors) / sizeof(char*);

static char* CONFIG_LOCATION;
static char* COLORS_LOCATION;
static struct map* config;
static char* config_path;
static char* stylesheet;
static char* color_path;
static uint8_t nyan_shift = 0;

struct option_node {
	char* option;
	struct wl_list link;
};

static char* get_exec_name(char* path) {
	char* slash = strrchr(path, '/');
	uint64_t offset;
	if(slash == NULL) {
		offset = 0;
	} else {
		offset = (slash - path) + 1;
	}
	return path + offset;
}

static void print_usage(char** argv) {
	printf("%s [options]\n", get_exec_name(argv[0]));
	printf("Options:\n");
	printf("--help\t\t\t-h\tDisplays this help message\n");
	printf("--fork\t\t\t-f\tForks the menu so you can close the terminal\n");
	printf("--conf\t\t\t-c\tSelects a config file to use\n");
	printf("--style\t\t\t-s\tSelects a stylesheet to use\n");
	printf("--color\t\t\t-C\tSelects a colors file to use\n");
	printf("--dmenu\t\t\t-d\tRuns in dmenu mode\n");
	printf("--show\t\t\t-S\tSpecifies the mode to run in. A list can be found in wofi(7)\n");
	printf("--width\t\t\t-W\tSpecifies the surface width\n");
	printf("--height\t\t-H\tSpecifies the surface height\n");
	printf("--prompt\t\t-p\tPrompt to display\n");
	printf("--xoffset\t\t-x\tThe x offset\n");
	printf("--yoffset\t\t-y\tThe y offset\n");
	printf("--normal-window\t\t-n\tRender to a normal window\n");
	printf("--allow-images\t\t-I\tAllows images to be rendered\n");
	printf("--allow-markup\t\t-m\tAllows pango markup\n");
	printf("--cache-file\t\t-k\tSets the cache file to use\n");
	printf("--term\t\t\t-t\tSpecifies the terminal to use when running in a term\n");
	printf("--password\t\t-P\tRuns in password mode\n");
	printf("--exec-search\t\t-e\tMakes enter always use the search contents not the first result\n");
	printf("--hide-scroll\t\t-b\tHides the scroll bars\n");
	printf("--matching\t\t-M\tSets the matching method, default is contains\n");
	printf("--insensitive\t\t-i\tAllows case insensitive searching\n");
	printf("--parse-search\t\t-q\tParses the search text removing image escapes and pango\n");
	printf("--version\t\t-v\tPrints the version and then exits\n");
	printf("--location\t\t-l\tSets the location\n");
	printf("--no-actions\t\t-a\tDisables multiple actions for modes that support it\n");
	printf("--define\t\t-D\tSets a config option\n");
	printf("--lines\t\t\t-L\tSets the height in number of lines\n");
	printf("--columns\t\t-w\tSets the number of columns to display\n");
	printf("--sort-order\t\t-O\tSets the sort order\n");
	printf("--bottom-search\t\t-B\tMove input entry under scroll\n");
	printf("--gtk-dark\t\t-G\tUses the dark variant of the current GTK theme\n");
	printf("--search\t\t-Q\tSearch for something immediately on open\n");
	printf("--monitor\t\t-o\tSets the monitor to open on\n");
	printf("--pre-display-cmd\t-r\tRuns command for the displayed entries, without changing the output. %%s for the real string\n");
	exit(0);
}

void wofi_load_css(bool nyan) {
	if(access(stylesheet, R_OK) == 0) {
		FILE* file = fopen(stylesheet, "r");
		fseek(file, 0, SEEK_END);
		ssize_t size = ftell(file);
		fseek(file, 0, SEEK_SET);
		char* data = malloc(size + 1);
		if (fread(data, 1, size, file) == 0) {
			fprintf(stderr, "failed to read stylesheet data from file\n");
			exit(EXIT_FAILURE);
		}
		fclose(file);

		data[size] = 0;
		struct wl_list lines;
		struct node {
			char* line;
			struct wl_list link;
		};
		wl_list_init(&lines);
		if(nyan) {
			for(ssize_t count = nyan_shift; count < 100 + nyan_shift; ++count) {
				size_t i = count % nyan_color_l;
				struct node* entry = malloc(sizeof(struct node));
				entry->line = strdup(nyan_colors[i]);
				wl_list_insert(&lines, &entry->link);
			}
			nyan_shift = (nyan_shift + 1) % nyan_color_l;
		} else {
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
		}

		ssize_t count = wl_list_length(&lines) - 1;
		if(count > 99) {
			fprintf(stderr, "Woah there that's a lot of colors. Try having no more than 100, thanks\n");
			exit(1);
		}
		struct node* node;
		wl_list_for_each(node, &lines, link) {
			//Do --wofi-color replace
			const char* color = node->line;
			const char* wofi_color = "--wofi-color";
			char count_str[3];
			snprintf(count_str, 3, "%zu", count--);
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

static void sig(int32_t signum) {
	switch(signum) {
	case SIGTERM:
		exit(1);
		break;
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
			.name = "prompt",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'p'
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
			.name = "normal-window",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'n'
		},
		{
			.name = "allow-images",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'I'
		},
		{
			.name = "allow-markup",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'm'
		},
		{
			.name = "cache-file",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'k'
		},
		{
			.name = "term",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 't'
		},
		{
			.name = "password",
			.has_arg = optional_argument,
			.flag = NULL,
			.val = 'P'
		},
		{
			.name = "exec-search",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'e'
		},
		{
			.name = "hide-scroll",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'b'
		},
		{
			.name = "matching",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'M'
		},
		{
			.name = "insensitive",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'i'
		},
		{
			.name = "parse-search",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'q'
		},
		{
			.name = "version",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'v'
		},
		{
			.name = "location",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'l'
		},
		{
			.name = "no-actions",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'a'
		},
		{
			.name = "define",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'D'
		},
		{
			.name = "lines",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'L'
		},
		{
			.name = "columns",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'w'
		},
		{
			.name = "sort-order",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'O'
		},
		{
			.name = "bottom-search",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'B'
		},
		{
			.name = "gtk-dark",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'G'
		},
		{
			.name = "search",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'Q'
		},
		{
			.name = "monitor",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'o'
		},
		{
			.name = "pre-display-cmd",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'r'
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
	char* normal_window = NULL;
	char* allow_images = NULL;
	char* allow_markup = NULL;
	char* cache_file = NULL;
	char* terminal = NULL;
	char* password_char = "false";
	char* exec_search = NULL;
	char* hide_scroll = NULL;
	char* matching = NULL;
	char* insensitive = NULL;
	char* parse_search = NULL;
	char* location = NULL;
	char* no_actions = NULL;
	char* lines = NULL;
	char* columns = NULL;
	char* sort_order = NULL;
	char* bottom_search = NULL;
	char* gtk_dark = NULL;
	char* search = NULL;
	char* monitor = NULL;
	char* pre_display_cmd = NULL;

	struct wl_list options;
	wl_list_init(&options);
	struct option_node* node;

	int opt;
	while((opt = getopt_long(argc, argv, "hfc:s:C:dS:W:H:p:x:y:nImk:t:P::ebM:iqvl:aD:L:w:O:BGQ:o:r:", opts, NULL)) != -1) {
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
		case 'n':
			normal_window = "true";
			break;
		case 'I':
			allow_images = "true";
			break;
		case 'm':
			allow_markup = "true";
			break;
		case 'k':
			cache_file = optarg;
			break;
		case 't':
			terminal = optarg;
			break;
		case 'P':
			password_char = optarg;
			break;
		case 'e':
			exec_search = "true";
			break;
		case 'b':
			hide_scroll = "true";
			break;
		case 'M':
			matching = optarg;
			break;
		case 'i':
			insensitive = "true";
			break;
		case 'q':
			parse_search = "true";
			break;
		case 'v':
			printf(VERSION"\n");
			exit(0);
			break;
		case 'l':
			location = optarg;
			break;
		case 'a':
			no_actions = "true";
			break;
		case 'D':
			node = malloc(sizeof(struct option_node));
			node->option = optarg;
			wl_list_insert(&options, &node->link);
			break;
		case 'L':
			lines = optarg;
			break;
		case 'w':
			columns = optarg;
			break;
		case 'O':
			sort_order = optarg;
			break;
		case 'B':
			bottom_search = "true";
			break;
		case 'G':
			gtk_dark = "true";
			break;
		case 'Q':
			search = optarg;
			break;
		case 'o':
			monitor = optarg;
			break;
		case 'r':
			pre_display_cmd = optarg;
			break;
		}
	}

	const char* home_dir = getenv("HOME");
	const char* wofi_conf = (getenv("WOFI_CONFIG_DIR") == NULL) ? getenv("XDG_CONFIG_HOME") : getenv("WOFI_CONFIG_DIR");
	if (wofi_conf == NULL)
	{	CONFIG_LOCATION = utils_concat(2, home_dir, "/.config/wofi"); }
	else
	{	CONFIG_LOCATION = utils_concat(2, wofi_conf, "/wofi"); }

	const char* wofi_cache = (getenv("WOFI_CACHE_DIR") == NULL) ? getenv("XDG_CACHE_HOME") : getenv("WOFI_CACHE_DIR");
	if (wofi_cache == NULL)
	{	COLORS_LOCATION = utils_concat(2, home_dir, "/.cache/wal/colors"); }
	else
	{	COLORS_LOCATION = utils_concat(2, wofi_cache, "/wal/colors"); }

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

	if(style_str == NULL) {
		style_str = map_get(config, "style");
	}

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

	if(color_str == NULL) {
		color_str = map_get(config, "color");
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

	if (bottom_search == NULL)
	{ bottom_search = map_get(config, "bottom_search"); }

	//Check if --gtk-dark was specified
	if(gtk_dark == NULL) {
		gtk_dark = map_get(config, "gtk_dark");
	}

	free(COLORS_LOCATION);

	struct option_node* tmp;
	wl_list_for_each_safe(node, tmp, &options, link) {
		config_put(config, node->option);
		wl_list_remove(&node->link);
		free(node);
	}

	if(map_get(config, "show") != NULL) {
		map_put(config, "mode", map_get(config, "show"));
	}

	if(strcmp(get_exec_name(argv[0]), "dmenu") == 0) {
		map_put(config, "mode", "dmenu");
		cache_file = "/dev/null";
	} else if(strcmp(get_exec_name(argv[0]), "wofi-askpass") == 0) {
		map_put(config, "mode", "dmenu");
		cache_file = "/dev/null";
		password_char = "*";
		prompt = "Password";
	} else if(mode != NULL) {
		map_put(config, "mode", mode);
	} else if(map_get(config, "mode") == NULL) {
		fprintf(stderr, "I need a mode, please give me a mode, that's what --show is for\n");
		exit(1);
	}

	map_put(config, "config_dir", CONFIG_LOCATION);

	if(width != NULL) {
		map_put(config, "width", width);
	}
	if(height != NULL) {
		map_put(config, "height", height);
	}
	if(prompt != NULL) {
		map_put(config, "prompt", prompt);
	}
	if(map_get(config, "xoffset") != NULL) {
		map_put(config, "x", map_get(config, "xoffset"));
	}
	if(x != NULL) {
		map_put(config, "x", x);
	}
	if(map_get(config, "yoffset") != NULL) {
		map_put(config, "y", map_get(config, "yoffset"));
	}
	if(y != NULL) {
		map_put(config, "y", y);
	}
	if(normal_window != NULL) {
		map_put(config, "normal_window", normal_window);
	}
	if(allow_images != NULL) {
		map_put(config, "allow_images", allow_images);
	}
	if(allow_markup != NULL) {
		map_put(config, "allow_markup", allow_markup);
	}
	if(cache_file != NULL) {
		map_put(config, "cache_file", cache_file);
	}
	if(terminal != NULL) {
		map_put(config, "term", terminal);
	}
	if(map_get(config, "password") != NULL) {
		map_put(config, "password_char", map_get(config, "password"));
	}
	if(password_char == NULL || (password_char != NULL && strcmp(password_char, "false") != 0)) {
		if(password_char == NULL) {
			password_char = "*";
		}
		map_put(config, "password_char", password_char);
	}
	if(exec_search != NULL) {
		map_put(config, "exec_search", exec_search);
	}
	if(hide_scroll != NULL) {
		map_put(config, "hide_scroll", hide_scroll);
	}
	if(matching != NULL) {
		map_put(config, "matching", matching);
	}
	if(insensitive != NULL) {
		map_put(config, "insensitive", insensitive);
	}
	if(parse_search != NULL) {
		map_put(config, "parse_search", parse_search);
	}
	if(location != NULL) {
		map_put(config, "location", location);
	}
	if(no_actions != NULL) {
		map_put(config, "no_actions", no_actions);
	}
	if(lines != NULL) {
		map_put(config, "lines", lines);
	}
	if(columns != NULL) {
		map_put(config, "columns", columns);
	}
	if(sort_order != NULL) {
		map_put(config, "sort_order", sort_order);
	}
	if(search != NULL) {
		map_put(config, "search", search);
	}
	if(monitor != NULL) {
		map_put(config, "monitor", monitor);
	}
	if(pre_display_cmd != NULL) {
		map_put(config, "pre_display_cmd", pre_display_cmd);
	}

	struct sigaction sigact = {0};
	sigact.sa_handler = sig;
	sigaction(SIGTERM, &sigact, NULL);


	gtk_init(&argc, &argv);

	if ((bottom_search != NULL) && (strcmp(bottom_search, "true") == 0))
	{ input_under_scroll = 1; }

	if(gtk_dark != NULL && strcmp(gtk_dark, "true") == 0) {
		g_object_set(gtk_settings_get_default(),
			"gtk-application-prefer-dark-theme", TRUE, NULL);
	}
	wofi_load_css(false);

	wofi_init(config);
	gtk_main();
	return 0;
}
