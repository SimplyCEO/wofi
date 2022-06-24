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

#ifndef UTIL_H
#define UTIL_H

#include <time.h>

#include <sys/types.h>

typedef double score_t;
#define SCORE_MAX INFINITY
#define SCORE_MIN -INFINITY
#define MATCH_FUZZY_MAX_LEN 256

time_t utils_get_time_millis(void);

void utils_sleep_millis(time_t millis);

char* utils_concat(size_t arg_count, ...);

size_t utils_min(size_t n1, size_t n2);

size_t utils_min3(size_t n1, size_t n2, size_t n3);

size_t utils_distance(const char* haystack, const char* needle);

score_t utils_fuzzy_score(const char *haystack, const char *needle);

void utils_mkdir(char *path, mode_t mode);

#endif
