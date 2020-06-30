/*
 *  Copyright (C) 2020 Scoopta
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

#ifndef WIDGET_BUILDER_H
#define WIDGET_BUILDER_H

#include <widget_builder_api.h>

#include <property_box.h>

struct widget_builder {
	WofiPropertyBox* box;
	struct widget* widget;
	struct mode* mode;
	size_t actions;
};

#endif
