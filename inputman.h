/*
 *  Copyright (C) 2001-2013  The Exult Team
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

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "mouse.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

class InputManager {
public:
	virtual ~InputManager() = default;
	virtual bool break_event_loop() const;
	virtual void handle_event(SDL_Event& event);
	void         handle_events();

private:
	uint32 last_rest       = 0;
	uint32 show_items_time = 0;
	sint32 show_items_x = 0, show_items_y = 0;
	sint32 left_down_x = 0, left_down_y = 0;
	sint32 joy_aim_x = 0, joy_aim_y = 0;
	sint32 joy_mouse_x = 0, joy_mouse_y = 0;

	Mouse::Avatar_Speed_Factors joy_speed_factor = Mouse::medium_speed_factor;

	bool dragging           = false;    // Object or gump being moved.
	bool dragged            = false;    // Flag for when obj. moved.
	bool right_on_gump      = false;    // Right clicked on gump?
	bool show_items_clicked = false;
};

#endif    // INPUT_MANAGER_H
