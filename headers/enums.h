/*
 *  ucmachine.cc - Interpreter for usecode.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ENUMS_H
#define ENUMS_H

#include "ignore_unused_variable_warning.h"
#include "istring.h"

#include <string>

using std::string_view_literals::operator""sv;

enum class UsecodeTrace {
	none,
	yes,
	verbose
};

constexpr std::string_view to_string(UsecodeTrace type) {
	switch (type) {
	case UsecodeTrace::none:
		return "none"sv;
	case UsecodeTrace::yes:
		return "yes"sv;
	case UsecodeTrace::verbose:
		return "verbose"sv;
	}
	return "none"sv;
}

constexpr UsecodeTrace from_string(
		std::string_view s, UsecodeTrace default_value) {
	ignore_unused_variable_warning(default_value);
	if (Pentagram::iequals(s, "yes")) {
		return UsecodeTrace::yes;
	}
	if (Pentagram::iequals(s, "verbose")) {
		return UsecodeTrace::verbose;
	}
	return UsecodeTrace::none;
}

enum class FillMode {
	Fill = 1,    ///< Game area fills all of the display surface
	Fit  = 2,    ///< Game area is stretched to the closest edge, maintaining
				///< 1:1 pixel aspect
	AspectCorrectFit = 3,    ///< Game area is stretched to the closest
							 ///< edge, with 1:1.2 pixel aspect
	FitAspectCorrect    = 3,
	Centre              = 4,    ///< Game area is centred
	AspectCorrectCentre = 5,    ///< Game area is centred and scaled to have
								///< 1:1.2 pixel aspect
	CentreAspectCorrect = 5,

	// Numbers higher than this incrementally scale by .5 more
	Centre_x1_5              = 6,
	AspectCorrectCentre_x1_5 = 7,
	Centre_x2                = 8,
	AspectCorrectCentre_x2   = 9,
	Centre_x2_5              = 10,
	AspectCorrectCentre_x2_5 = 11,
	Centre_x3                = 12,
	AspectCorrectCentre_x3   = 13,
	// And so on....

	// Arbitrarty scaling support => (x<<16)|y
	Centre_640x480
			= (640 << 16) | 480    ///< Scale to specific dimensions and centre
};

std::string to_string(FillMode type);

FillMode from_string(std::string_view s, FillMode default_value);

inline int operator+(FillMode a, int b) {
	return static_cast<int>(a) + b;
}

inline int operator-(FillMode a, FillMode b) {
	return static_cast<int>(a) - static_cast<int>(b);
}

inline int operator&(FillMode a, int b) {
	return static_cast<int>(a) & b;
}

#endif
