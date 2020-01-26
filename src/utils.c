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

#include <utils.h>

time_t utils_get_time_millis(void) {
	struct timeval time;
	gettimeofday(&time, NULL);
	return (time.tv_sec * 1000) + (time.tv_usec / 1000);
}

void utils_sleep_millis(time_t millis) {
	struct timespec time;
	time.tv_sec = millis / 1000;
	time.tv_nsec = (millis % 1000) * pow(1000, 2);
	nanosleep(&time, NULL);
}

char* utils_concat(size_t arg_count, ...) {
	va_list args;
	va_start(args, arg_count);
	size_t buf_s = 1;
	for(size_t count = 0; count < arg_count; ++count) {
		buf_s += strlen(va_arg(args, char*));
	}
	va_end(args);
	va_start(args, arg_count);
	char* buffer = malloc(buf_s);
	strcpy(buffer, va_arg(args, char*));
	for(size_t count = 0; count < arg_count - 1; ++count) {
		strcat(buffer, va_arg(args, char*));
	}
	va_end(args);
	return buffer;
}

size_t utils_min(size_t n1, size_t n2) {
	if(n1 < n2) {
		return n1;
	} else {
		return n2;
	}
}

size_t utils_min3(size_t n1, size_t n2, size_t n3) {
	if(n1 < n2 && n1 < n3) {
		return n1;
	} else if(n2 < n1 && n2 < n3) {
		return n2;
	} else {
		return n3;
	}
}

size_t utils_distance(const char* str1, const char* str2) {
	size_t str1_len = strlen(str1);
	size_t str2_len = strlen(str2);

	size_t arr[str1_len + 1][str2_len + 1];
	arr[0][0] = 0;
	for(size_t count = 1; count <= str1_len; ++count) {
		arr[count][0] = count;
	}
	for(size_t count = 1; count <= str2_len; ++count) {
		arr[0][count] = count;
	}

	uint8_t cost;
	for(size_t c1 = 1; c1 <= str1_len; ++c1) {
		for(size_t c2 = 1; c2 <= str2_len; ++c2) {
			if(str1[c1 - 1] == str2[c2 - 1]) {
				cost = 0;
			} else {
				cost = 1;
			}
			arr[c1][c2] = utils_min3(arr[c1 - 1][c2] + 1, arr[c1][c2 - 1] + 1, arr[c1 - 1][c2 - 1] + cost);
		}
	}

	if(strstr(str1, str2) != NULL) {
		arr[str1_len][str2_len] -= str2_len;
	}

	return arr[str1_len][str2_len];
}
