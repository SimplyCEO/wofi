/*
 *  Copyright (C) 2019-2022 Scoopta
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

#ifndef MATCH_H
#define MATCH_H

#include <math.h>
#include <stdbool.h>

typedef double score_t;
#define SCORE_MAX INFINITY
#define SCORE_MIN -INFINITY
#define MATCH_FUZZY_MAX_LEN 256
#define MAX_MULTI_CONTAINS_FILTER_SIZE 256

enum matching_mode {
  MATCHING_MODE_CONTAINS,
  MATCHING_MODE_MULTI_CONTAINS,
  MATCHING_MODE_FUZZY
};

int sort_for_matching_mode(const char *text1, const char *text2, int fallback,
                           enum matching_mode match_type, const char *filter, bool insensitive);

bool match_for_matching_mode(const char* filter, const char* text, enum matching_mode matching, bool insensitive);
#endif
