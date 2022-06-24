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

#include <ctype.h>
#include <libgen.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>

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
	return n1 < n2 ? n1 : n2;
}

size_t utils_max(size_t n1, size_t n2) {
	return n1 > n2 ? n1 : n2;
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

size_t utils_distance(const char* haystack, const char* needle) {
	size_t str1_len = strlen(haystack);
	size_t str2_len = strlen(needle);

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
			if(haystack[c1 - 1] == needle[c2 - 1]) {
				cost = 0;
			} else {
				cost = 1;
			}
			arr[c1][c2] = utils_min3(arr[c1 - 1][c2] + 1, arr[c1][c2 - 1] + 1, arr[c1 - 1][c2 - 1] + cost);
		}
	}

	if(strstr(haystack, needle) != NULL) {
		arr[str1_len][str2_len] -= str2_len;
	}

	return arr[str1_len][str2_len];
}

// leading gap
#define SCORE_GAP_LEADING -0.005
// trailing gap
#define SCORE_GAP_TRAILING -0.005
// gap in the middle
#define SCORE_GAP_INNER -0.01
// we matched the characters consecutively
#define SCORE_MATCH_CONSECUTIVE 1.0
// we got a consecutive match, but insensitive is on
// and we didn't match the case.
#define SCORE_MATCH_NOT_MATCH_CASE 0.9
// we are matching after a slash
#define SCORE_MATCH_SLASH 0.9
// we are matching after a space dash or hyphen
#define SCORE_MATCH_WORD 0.8
// we are matching a camel case letter
#define SCORE_MATCH_CAPITAL 0.7
// we are matching after a dot
#define SCORE_MATCH_DOT 0.6

#define SWAP(x, y, T)													\
	do {																\
		T SWAP = x;														\
		x = y;															\
		y = SWAP;														\
  } while (0)

#define max(a, b) (((a) > (b)) ? (a) : (b))

static void precompute_bonus(const char *haystack, score_t *match_bonus) {
	/* Which positions are beginning of words */
	int m = strlen(haystack);
	char last_ch = '\0';
	for (int i = 0; i < m; i++) {
		char ch = haystack[i];

		score_t score = 0;
		if (isalnum(ch)) {
			if (!last_ch || last_ch == '/') {
				score = SCORE_MATCH_SLASH;
			} else if (last_ch == '-' || last_ch == '_' ||
					   last_ch == ' ') {
				score = SCORE_MATCH_WORD;
			} else if (last_ch >= 'a' && last_ch <= 'z' &&
					   ch >= 'A' && ch <= 'Z') {
				/* CamelCase */
				score = SCORE_MATCH_CAPITAL;
			} else if (last_ch == '.') {
				score = SCORE_MATCH_DOT;
			}
		}

		match_bonus[i] = score;
		last_ch = ch;
	}
}

static inline bool match_with_case(char a, char b, bool insensitive) {
	if(insensitive) {
		return tolower(a) == tolower(b);
	} else {
		return a == b;
	}
}

static inline void match_row(int row, score_t* curr_D, score_t* curr_M,
							 const score_t* last_D, const score_t * last_M,
							 const char* needle, const char* haystack, int n, int m, score_t* match_bonus) {
	int i = row;

	score_t prev_score = SCORE_MIN;
	score_t gap_score = i == n - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

	for (int j = 0; j < m; j++) {
		if (match_with_case(needle[i], haystack[j], true)) {
			score_t score = SCORE_MIN;
			if (!i) {
				// first line we fill in a row for non-matching
				score = (j * SCORE_GAP_LEADING) + match_bonus[j];
			} else if (j) { /* i > 0 && j > 0*/
				// we definitely match case insensitively already so if
				//  our character isn't the same then we have a
				//  different case
				score_t consecutive_bonus = needle[i] == haystack[j] ? SCORE_MATCH_CONSECUTIVE : SCORE_MATCH_NOT_MATCH_CASE;

				score = max(last_M[j - 1] + match_bonus[j],
							/* consecutive match, doesn't stack
							   with match_bonus */
							last_D[j - 1] + consecutive_bonus);
			}
			curr_D[j] = score;
			curr_M[j] = prev_score = max(score, prev_score + gap_score);
		} else {
			curr_D[j] = SCORE_MIN;
			curr_M[j] = prev_score = prev_score + gap_score;
		}
	}
}

// Fuzzy matching scoring. Adapted from
// https://github.com/jhawthorn/fzy/blob/master/src/match.c and
// https://github.com/jhawthorn/fzy/blob/master/ALGORITHM.md
// For a fuzzy match string needle being searched for in haystack we provide a
// number score for how well we match.
// We create two matrices of size needle_len (n) by haystack_len (m).
// The first matrix is the score matrix. Each position (i,j) within this matrix
// consists of the score that corresponds to the score that would be generated
// by matching the first i characters of the needle with the first j
// characters of the haystack. Gaps have a fixed penalty for having a gap along
// with a linear penalty for gap size (c.f. gotoh's algorithm).
// matches give a positive score, with a slight weight given to matches after
// certain special characters (i.e. the first character after a `/` will be
// "almost" consecutive but lower than an actual consecutive match).
// Our second matrix is our diagonal matrix where we store the best match
// that ends at a match. This allows us to calculate our gap penalties alongside
// our consecutive match scores.
// In addition, since we only rely on the current, and previous row of the
// matrices and we only want to compute the score, we only store those scores
// and reuse the previous rows (rather than storing the entire (n*m) matrix).
// In addition we've simplified some of the algorithm compared to fzy to
// improve legibility. (Can reimplement lookup tables later if wanted.)
// Also, the reference algorithm does not take into account case sensitivity
// which has been implemented here.


score_t utils_fuzzy_score(const char* haystack, const char* needle) {
	if(!*needle)
		return SCORE_MIN;

	int n = strlen(needle);
	int m = strlen(haystack);
	score_t match_bonus[m];
	precompute_bonus(haystack, match_bonus);

	if(m > MATCH_FUZZY_MAX_LEN || n > m) {
		/*
		 * Unreasonably large candidate: return no score
		 * If it is a valid match it will still be returned, it will
		 * just be ranked below any reasonably sized candidates
		 */
		return SCORE_MIN;
	} else if(n == m) {
		/* Since this method can only be called with a haystack which
		 * matches needle. If the lengths of the strings are equal the
		 * strings themselves must also be equal (ignoring case).
		 */
		return SCORE_MAX;
	}

	/*
	 * D[][] Stores the best score for this position ending with a match.
	 * M[][] Stores the best possible score at this position.
	 */
	score_t D[2][MATCH_FUZZY_MAX_LEN], M[2][MATCH_FUZZY_MAX_LEN];

	score_t* last_D, *last_M;
	score_t* curr_D, *curr_M;

	last_D = D[0];
	last_M = M[0];
	curr_D = D[1];
	curr_M = M[1];

	for (int i = 0; i < n; i++) {
		match_row(i, curr_D, curr_M, last_D, last_M, needle, haystack, n, m, match_bonus);

		SWAP(curr_D, last_D, score_t *);
		SWAP(curr_M, last_M, score_t *);
	}

	return last_M[m - 1];
}

void utils_mkdir(char* path, mode_t mode) {
	if(access(path, F_OK) != 0) {
		char* tmp = strdup(path);
		utils_mkdir(dirname(tmp), mode);
		mkdir(path, mode);
		free(tmp);
	}
}
