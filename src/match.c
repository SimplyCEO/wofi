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

#include <ctype.h>
#include <match.h>
#include <string.h>

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
	} while(0)

#define max(a, b) (((a) > (b)) ? (a) : (b))

// matching
static bool contains_match(const char* filter, const char* text, bool insensitive) {
	if(filter == NULL || strcmp(filter, "") == 0) {
		return true;
	}
	if(text == NULL) {
		return false;
	}
	if(insensitive) {
		return strcasestr(text, filter) != NULL;
	} else {
		return strstr(text, filter) != NULL;
	}
}

static char* strcasechr(const char* s,char c, bool insensitive) {
	if(insensitive) {
		const char accept[3] = {c, toupper(c), 0};
		return strpbrk(s, accept);
	} else {
		return strchr(s, c);
	}
}

static bool fuzzy_match(const char* filter, const char* text, bool insensitive) {
	if(filter == NULL || strcmp(filter, "") == 0) {
		return true;
	}
	if(text == NULL) {
		return false;
	}
	// we just check that all the characters (ignoring case) are in the
	// search text possibly case insensitively in the correct order
	while(*filter != 0) {
		char nch = *filter++;

		if(!(text = strcasechr(text, nch, insensitive))) {
			return false;
		}
		text++;
	}
	return true;
}

static bool multi_contains_match(const char* filter, const char* text, bool insensitive) {
	if(filter == NULL || strcmp(filter, "") == 0) {
		return true;
	}
	if(text == NULL) {
		return false;
	}
	char new_filter[MAX_MULTI_CONTAINS_FILTER_SIZE];
	strncpy(new_filter, filter, sizeof(new_filter));
	new_filter[sizeof(new_filter) - 1] = '\0';
	char* token;
	char* rest = new_filter;
	while((token = strtok_r(rest, " ", &rest))) {
		if(contains_match(token, text, insensitive) == false) {
			return false;
		}
	}
	return true;
}

bool match_for_matching_mode(const char* filter, const char* text,
							 enum matching_mode matching, bool insensitive) {
	bool retval;
	switch(matching) {
	case MATCHING_MODE_MULTI_CONTAINS:
		retval = multi_contains_match(filter, text, insensitive);
		break;
	case MATCHING_MODE_CONTAINS:
		retval = contains_match(filter, text, insensitive);
		break;
	case MATCHING_MODE_FUZZY:
		retval = fuzzy_match(filter, text, insensitive);
		break;
	default:
		return false;
	}
	return retval;
}

// end matching

// fuzzy matching
static void precompute_bonus(const char* haystack, score_t* match_bonus) {
	/* Which positions are beginning of words */
	int m = strlen(haystack);
	char last_ch = '\0';
	for(int i = 0; i < m; i++) {
		char ch = haystack[i];

		score_t score = 0;
		if(isalnum(ch)) {
			if(!last_ch || last_ch == '/') {
				score = SCORE_MATCH_SLASH;
			} else if(last_ch == '-' || last_ch == '_' ||
					   last_ch == ' ') {
				score = SCORE_MATCH_WORD;
			} else if(last_ch >= 'a' && last_ch <= 'z' &&
					   ch >= 'A' && ch <= 'Z') {
				/* CamelCase */
				score = SCORE_MATCH_CAPITAL;
			} else if(last_ch == '.') {
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
							 const score_t* last_D, const score_t* last_M,
							 const char* needle, const char* haystack, int n, int m, score_t* match_bonus, bool insensitive) {
	int i = row;

	score_t prev_score = SCORE_MIN;
	score_t gap_score = i == n - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

	for(int j = 0; j < m; j++) {
		if(match_with_case(needle[i], haystack[j], insensitive)) {
			score_t score = SCORE_MIN;
			if(!i) {
				// first line we fill in a row for non-matching
				score = (j * SCORE_GAP_LEADING) + match_bonus[j];
			} else if(j) { /* i > 0 && j > 0*/
				// we definitely match case insensitively already so if
				// our character isn't the same then we have a different case
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

static score_t fuzzy_score(const char* haystack, const char* needle, bool insensitive) {
	if(*needle == 0)
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

	for(int i = 0; i < n; i++) {
		match_row(i, curr_D, curr_M, last_D, last_M, needle, haystack, n, m, match_bonus, insensitive);

		SWAP(curr_D, last_D, score_t *);
		SWAP(curr_M, last_M, score_t *);
	}

	return last_M[m - 1];
}
// end fuzzy matching

// sorting
static int fuzzy_sort(const char* text1, const char* text2, const char* filter, bool insensitive) {
	bool match1 = fuzzy_match(filter, text1, insensitive);
	bool match2 = fuzzy_match(filter, text2, insensitive);
	// both filters match do fuzzy scoring
	if(match1 && match2) {
		score_t dist1 = fuzzy_score(text1, filter, insensitive);
		score_t dist2 = fuzzy_score(text2, filter, insensitive);
		if(dist1 == dist2) {
			// same same
			return 0;
		} else if(dist1 > dist2) { // highest score wins.
			// text1 goes first
			return -1;
		} else {
			// text2 goes first
			return 1;
		}
	} else if(match1) {
		// text1 goes first
		return -1;
	} else if(match2) {
		// text2 goes first
		return 1;
	} else {
		// same same.
		return 0;
	}
}

// we sort based on how early in the string all the matches are.
// if there are matches for each.
static int multi_contains_sort(const char* text1, const char* text2, const char* filter, bool insensitive) {
	// sum of string positions of each match
	int t1_count = 0;
	int t2_count = 0;
	// does this string match with mult-contains
	bool t1_match = true;
	bool t2_match = true;

	char new_filter[MAX_MULTI_CONTAINS_FILTER_SIZE];
	strncpy(new_filter, filter, sizeof(new_filter));
	new_filter[sizeof(new_filter) - 1] = '\0';

	char* token;
	char* rest = new_filter;
	while((token = strtok_r(rest, " ", &rest))) {
		char* str1, *str2;
		if(insensitive) {
			str1 = strcasestr(text1, token);
			str2 = strcasestr(text2, token);
		} else {
			str1 = strstr(text1, token);
			str2 = strstr(text2, token);
		}
		t1_match = t1_match && str1 != NULL;
		t2_match = t2_match && str2 != NULL;
		if(str1 != NULL) {
			int pos1 = str1 - text1;
			t1_count += pos1;
		}
		if(str2 != NULL) {
			int pos2 = str2 - text2;
			t2_count += pos2;
		}
	}
	if(t1_match && t2_match) {
		// both match
		// return the one with the smallest count.
		return t1_count - t2_count;
	} else if(t1_match) {
		return -1;
	} else if(t2_match) {
		return 1;
	} else {
		return 0;
	}
}
static int contains_sort(const char* text1, const char* text2, const char* filter, bool insensitive) {
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

int sort_for_matching_mode(const char* text1, const char* text2, int fallback,
						   enum matching_mode match_type, const char* filter, bool insensitive) {
	int primary = 0;
	switch(match_type) {
	case MATCHING_MODE_MULTI_CONTAINS:
		primary = multi_contains_sort(text1, text2, filter, insensitive);
		break;
	case MATCHING_MODE_CONTAINS:
		primary = contains_sort(text1, text2, filter, insensitive);
		break;
		case MATCHING_MODE_FUZZY:
			primary = fuzzy_sort(text1, text2, filter, insensitive);
			break;
	default:
		return 0;
	}
	if(primary == 0) {
		return fallback;
	}
	return primary;
}
// end sorting

