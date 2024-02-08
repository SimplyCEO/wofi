/*
 *  Copyright (C) 2019-2024 Scoopta
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

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#include <map.h>

void config_put(struct map* map, char* line);

void config_load(struct map* map, const char* config);

char* config_get(struct map* config, const char* key, char* def_opt);

int config_get_mnemonic(struct map* config, const char* key, char* def_opt, int num_choices, ...);

#endif
