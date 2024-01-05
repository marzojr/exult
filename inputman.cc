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

#include "inputman.h"

#include "ShortcutBar_gump.h"
#include "actors.h"
#include "game.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "party.h"
#include "touchui.h"
#include "tqueue.h"

#include <SDL_events.h>

void InputManager::handle_event(SDL_Event& event) {
	ignore_unused_variable_warning(event);
	switch (static_cast<SDL_EventType>(event.type)) {
	case SDL_CONTROLLERAXISMOTION:
		break;

	// Keystroke.
	case SDL_KEYDOWN:
		break;

	// Keystroke.
	case SDL_KEYUP:
		break;

	case SDL_TEXTINPUT:
		break;

	case SDL_MOUSEBUTTONDOWN:
		break;

	case SDL_MOUSEBUTTONUP:
		break;

	case SDL_MOUSEMOTION:
		break;

	// Mousewheel scrolling of view port with SDL2.
	case SDL_MOUSEWHEEL:
		break;

	case SDL_FINGERDOWN:
		break;

	// two-finger scrolling of view port with SDL2.
	case SDL_FINGERMOTION:
		break;

	case SDL_DROPFILE:
		break;

	case SDL_APP_WILLENTERBACKGROUND:
		break;

	case SDL_WINDOWEVENT:
		break;

	case SDL_QUIT:
		break;

	default:
		if (event.type == TouchUI::eventType) {
		} else if (event.type == ShortcutBar_gump::eventType) {
		}
		break;
	}
}

void InputManager::handle_events() {
	SDL_Event event;
	while (!break_event_loop() && SDL_PollEvent(&event) != 0) {
		handle_event(event);
	}

	Game_window* gwin  = Game_window::get_instance();
	auto         ticks = Game::get_ticks();
	// Animate unless dormant.
	if (gwin->have_focus() && !dragging) {
		gwin->get_tqueue()->activate(ticks);
	}

	// Moved this out of the animation loop, since we want movement to be
	// more responsive. Also, if the step delta is only 1 tile,
	// always check every loop
	if ((!gwin->is_moving() || gwin->get_step_tile_delta() == 1)
		&& gwin->main_actor_can_act_charmed()) {
		int       x;
		int       y;    // Check for 'stuck' Avatar.
		const int ms = SDL_GetMouseState(&x, &y);
		// mouse movement needs to be adjusted for HighDPI
		gwin->get_win()->screen_to_game_hdpi(x, y, gwin->get_fastmouse(), x, y);
		if ((SDL_BUTTON(3) & ms) != 0 && !right_on_gump) {
			gwin->start_actor(x, y, Mouse::mouse->avatar_speed);
		} else if (ticks > last_rest) {
			const int resttime = ticks - last_rest;
			gwin->get_main_actor()->resting(resttime);

			Party_manager* party_man = gwin->get_party_man();
			const int      cnt       = party_man->get_count();
			for (int i = 0; i < cnt; i++) {
				const int party_member = party_man->get_member(i);
				Actor*    person       = gwin->get_npc(party_member);
				if (person == nullptr) {
					continue;
				}
				person->resting(resttime);
			}
			last_rest = ticks;
		}
	}

	// handle delayed showing of clicked items (wait for possible dblclick)
	if (show_items_clicked && ticks > show_items_time) {
		gwin->show_items(show_items_x, show_items_y, false);
		show_items_clicked = false;
	}

	if (joy_aim_x != 0 || joy_aim_y != 0) {
		// Calculate the player speed
		const int speed = 200 * gwin->get_std_delay()
						  / static_cast<int>(joy_speed_factor);

		// [re]start moving
		gwin->start_actor(joy_aim_x, joy_aim_y, speed);
	}

	if (joy_mouse_x != 0 || joy_mouse_y != 0) {
		int x;
		int y;
		SDL_GetMouseState(&x, &y);
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_TRUE);
		SDL_WarpMouseInWindow(nullptr, x + joy_mouse_x, y + joy_mouse_y);
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
	}
}
