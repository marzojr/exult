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
#include "touchui.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <cmath>

namespace {
	template <
			typename T,
			std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
	inline bool isZero(T val) noexcept {
		const int result = std::fpclassify(val);
		return result == FP_SUBNORMAL || result == FP_ZERO;
	}

	template <
			typename T1, typename T2,
			std::enable_if_t<
					std::is_floating_point_v<T1>
							&& std::is_floating_point_v<T2>,
					bool>
			= true>
	inline bool floatCompare(T1 f1, T2 f2) noexcept {
		using T = std::common_type_t<T1, T2>;
		T v1    = f1;
		T v2    = f2;
		return isZero(v1 - v2);
	}
}    // namespace

bool AxisVector::isNonzero() const noexcept {
	return !isZero(x) || !isZero(y);
}

void InputManager::handle_axis_motion(SDL_ControllerAxisEvent& event) noexcept {
	SDL_GameController* input_device
			= SDL_GameControllerFromInstanceID(event.which);
	if (input_device == nullptr) {
		// Somehow we got an event for a non-existent gamepad?
		return;
	}
	auto get_normalized_axis = [input_device](SDL_GameControllerAxis axis) {
		// Dead-zone is applied to each axis, X and Y, on the game's 2d
		// plane. All axis readings below this are ignored
		constexpr const float axis_dead_zone = 0.25f;
		float value = SDL_GameControllerGetAxis(input_device, axis)
					  / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
		// Many analog game-controllers report non-zero axis values,
		// even when the controller isn't moving.  These non-zero
		// values can change over time, as the thumb-stick is moved
		// by the player.
		//
		// In order to prevent idle controller sticks from leading
		// to unwanted movements, axis-values that are small will
		// be ignored.  This is sometimes referred to as a
		// "dead zone".
		if (axis_dead_zone >= std::fabs(value) || isZero(value)) {
			value = 0.0f;
		}
		return value;
	};

	switch (static_cast<SDL_GameControllerAxis>(event.axis)) {
	case SDL_CONTROLLER_AXIS_LEFTX:
	case SDL_CONTROLLER_AXIS_LEFTY: {
		joy_aim
				= {get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTX),
				   get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTY)};
		invoke_callback(
				gamepadAxisCallbacks, AxisEventKind::GamepadLeftAxis, joy_aim);
		break;
	}
	case SDL_CONTROLLER_AXIS_RIGHTX:
	case SDL_CONTROLLER_AXIS_RIGHTY: {
		joy_mouse
				= {get_normalized_axis(SDL_CONTROLLER_AXIS_RIGHTX),
				   get_normalized_axis(SDL_CONTROLLER_AXIS_RIGHTY)};
		invoke_callback(
				gamepadAxisCallbacks, AxisEventKind::GamepadRightAxis,
				joy_mouse);
		break;
	}
	case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
	case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: {
		joy_rise
				= {get_normalized_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT),
				   get_normalized_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)};
		invoke_callback(
				gamepadAxisCallbacks, AxisEventKind::GamepadTrigger, joy_rise);
		break;
	}
	case SDL_CONTROLLER_AXIS_INVALID:
	case SDL_CONTROLLER_AXIS_MAX:
		break;
	}
}

void InputManager::handle_event(SDL_Event& event) noexcept {
	ignore_unused_variable_warning(event);
	switch (static_cast<SDL_EventType>(event.type)) {
	case SDL_CONTROLLERAXISMOTION:
		handle_axis_motion(event.caxis);
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

void InputManager::handle_events() noexcept {
	SDL_Event event;
	while (!invoke_callback(breakLoopCallbacks) && SDL_PollEvent(&event) != 0) {
		handle_event(event);
	}

	Game_window* gwin = Game_window::get_instance();
	// Note: always do this. This joypad axis is the equivalent of the mouse.
	if (joy_mouse.isNonzero()) {
		int x;
		int y;
		SDL_GetMouseState(&x, &y);
		x += static_cast<int>(std::floor(joy_mouse.x));
		y += static_cast<int>(std::floor(joy_mouse.y));
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_TRUE);
		SDL_WarpMouseInWindow(nullptr, x, y);
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
	}
}
