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

#include "eventman.h"

#include "ShortcutBar_gump.h"
#include "actors.h"
#include "game.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "mouse.h"
#include "touchui.h"

#include <SDL_video.h>

#include <cmath>
#include <iostream>
#include <memory>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#ifdef USE_EXULTSTUDIO /* Only needed for communication with exult studio */
#	ifdef _WIN32
#		include "windrag.h"
#	endif

#	include "server.h"

#	include <SDL_syswm.h>

void Move_dragged_shape(
		int shape, int frame, int x, int y, int prevx, int prevy, bool show);
#	ifdef _WIN32
void Move_dragged_combo(
		int xtiles, int ytiles, int tiles_right, int tiles_below, int x, int y,
		int prevx, int prevy, bool show);
#	endif
void Drop_dragged_shape(int shape, int frame, int x, int y);
void Drop_dragged_chunk(int chunknum, int x, int y);
void Drop_dragged_npc(int npcnum, int x, int y);
void Drop_dragged_combo(int cnt, U7_combo_data* combo, int x, int y);
#endif

namespace {
	template <
			typename T,
			std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
	[[maybe_unused]] inline bool isZero(T val) noexcept {
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
	[[maybe_unused]] inline bool floatCompare(T1 f1, T2 f2) noexcept {
		using T = std::common_type_t<T1, T2>;
		T v1    = f1;
		T v2    = f2;
		return isZero(v1 - v2);
	}
}    // namespace

bool AxisVector::isNonzero() const noexcept {
	return !isZero(x) || !isZero(y);
}

bool AxisTrigger::isNonzero() const noexcept {
	return !isZero(left) || !isZero(right);
}

bool FingerMotion::isNonzero() const noexcept {
	return !isZero(x) || !isZero(y);
}

MousePosition::MousePosition() {
	SDL_GetMouseState(&x, &y);
}

MousePosition::MousePosition(int x_, int y_) : x(x_), y(y_) {}

#if defined(USE_EXULTSTUDIO) && defined(_WIN32)
struct OleDeleter {
	void operator()(Windnd* drag) {
		if (drag != nullptr) {
			drag->Release();
		}
	}
};

using WindndPtr = std::unique_ptr<Windnd, OleDeleter>;
#endif

class EventManagerImpl : public EventManager {
public:
	EventManagerImpl();
	void handle_events() override;
	void enable_dropfile() override;
	void disable_dropfile() override;

private:
	bool break_event_loop() const;
	void handle_event(SDL_Event& event);
	void handle_event(SDL_ControllerDeviceEvent& event) noexcept;
	void handle_event(SDL_KeyboardEvent& event) noexcept;
	void handle_event(SDL_TextInputEvent& event) noexcept;
	void handle_event(SDL_MouseMotionEvent& event) noexcept;
	void handle_event(SDL_MouseButtonEvent& event) noexcept;
	void handle_event(SDL_MouseWheelEvent& event) noexcept;
	void handle_event(SDL_TouchFingerEvent& event) noexcept;
	void handle_event(SDL_DropEvent& event) noexcept;
	void handle_event(SDL_WindowEvent& event) noexcept;

	void handle_background_event();
	void handle_quit_event();

	void handle_gamepad_axis_input() noexcept;

	SDL_GameController* find_controller() const noexcept;
	SDL_GameController* open_game_controller(int joystick_index) const noexcept;

	SDL_GameController* active_gamepad = nullptr;

#if defined(USE_EXULTSTUDIO) && defined(_WIN32)
	WindndPtr windnd{nullptr, OleDeleter{}};
	HWND      hgwin{};
#endif

	template <typename Callback_t, typename... Ts>
	std::invoke_result_t<Callback_t, Ts...> invoke_callback(
			const CallbackStack<Callback_t>& stack,
			Ts&&... args) const noexcept {
		if (!stack.empty()) {
			const auto& callback = stack.top();
			return callback(std::forward<Ts>(args)...);
		}
		using Result = std::invoke_result_t<Callback_t, Ts...>;
		if constexpr (!std::is_same_v<void, std::decay_t<Result>>) {
			return Result{};
		}
	}
};

SDL_GameController* EventManagerImpl::open_game_controller(
		int joystick_index) const noexcept {
	SDL_GameController* input_device = SDL_GameControllerOpen(joystick_index);
	if (input_device != nullptr) {
		SDL_GameControllerGetJoystick(input_device);
		std::cout << "Game controller attached and open: \""
				  << SDL_GameControllerName(input_device) << '"' << std::endl;
	} else {
		std::cout
				<< "Game controller attached, but it failed to open. Error: \""
				<< SDL_GetError() << '"' << std::endl;
	}
	return input_device;
}

SDL_GameController* EventManagerImpl::find_controller() const noexcept {
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i) != 0u) {
			return open_game_controller(i);
		}
	}
	// No gamepads found.
	return nullptr;
}

EventManagerImpl::EventManagerImpl() {
	active_gamepad = find_controller();
}

void EventManagerImpl::handle_gamepad_axis_input() noexcept {
	if (active_gamepad == nullptr) {
		// Exit if no active gamepad
		return;
	}
	constexpr const float axis_dead_zone = 0.25f;
	constexpr const float mouse_scale    = 2.f / axis_dead_zone;

	auto get_normalized_axis = [&](SDL_GameControllerAxis axis) {
		// Dead-zone is applied to each axis, X and Y, on the game's 2d
		// plane. All axis readings below this are ignored
		float value = SDL_GameControllerGetAxis(active_gamepad, axis);
		value /= static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
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

	const AxisVector joy_aim{
			get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTX),
			get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTY)};
	const AxisVector joy_mouse{
			get_normalized_axis(SDL_CONTROLLER_AXIS_RIGHTX),
			get_normalized_axis(SDL_CONTROLLER_AXIS_RIGHTY)};
	const AxisTrigger joy_rise{
			get_normalized_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT),
			get_normalized_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)};

	if (joy_mouse.isNonzero()) {
		// This joypad axis is the equivalent of the mouse.
		Game_window* gwin  = Game_window::get_instance();
		const int    scale = gwin->get_win()->get_scale_factor();

		int x;
		int y;
		SDL_GetMouseState(&x, &y);
		int delta_x = static_cast<int>(
				round(mouse_scale * joy_mouse.x * static_cast<float>(scale)));
		int delta_y = static_cast<int>(
				round(mouse_scale * joy_mouse.y * static_cast<float>(scale)));
		x += delta_x;
		y += delta_y;
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_TRUE);
		SDL_WarpMouseInWindow(nullptr, x, y);
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
	}

	invoke_callback(gamepadAxisCallbacks, joy_aim, joy_mouse, joy_rise);
}

bool EventManagerImpl::break_event_loop() const {
	return invoke_callback(breakLoopCallbacks);
}

void EventManagerImpl::handle_event(SDL_ControllerDeviceEvent& event) noexcept {
	switch (event.type) {
	case SDL_CONTROLLERDEVICEADDED: {
		// If we are already using a gamepad, skip.
		if (active_gamepad == nullptr) {
			const SDL_JoystickID joystick_id
					= SDL_JoystickGetDeviceInstanceID(event.which);
			if (SDL_GameControllerFromInstanceID(joystick_id) == nullptr) {
				active_gamepad = open_game_controller(event.which);
			}
		}
		break;
	}
	case SDL_CONTROLLERDEVICEREMOVED: {
		// If the gamepad we are using was removed, we need a new one.
		if (active_gamepad != nullptr
			&& event.which
					   == SDL_JoystickInstanceID(
							   SDL_GameControllerGetJoystick(active_gamepad))) {
			std::cout << "Game controller \""
					  << SDL_GameControllerName(active_gamepad)
					  << "\" detached and closed." << std::endl;
			SDL_GameControllerClose(active_gamepad);
			active_gamepad = find_controller();
		}
		break;
	}
	default:
		break;
	}
}

void EventManagerImpl::handle_event(SDL_KeyboardEvent& event) noexcept {
	switch (event.type) {
	case SDL_KEYDOWN:
		break;

	case SDL_KEYUP:
		break;

	default:
		break;
	}
}

void EventManagerImpl::handle_event(SDL_TextInputEvent& event) noexcept {
	ignore_unused_variable_warning(event);
}

void EventManagerImpl::handle_event(SDL_MouseMotionEvent& event) noexcept {
	ignore_unused_variable_warning(event);
}

void EventManagerImpl::handle_event(SDL_MouseButtonEvent& event) noexcept {
	switch (event.type) {
	case SDL_MOUSEBUTTONDOWN:
		break;

	case SDL_MOUSEBUTTONUP:
		break;

	default:
		break;
	}
}

void EventManagerImpl::handle_event(SDL_MouseWheelEvent& event) noexcept {
	ignore_unused_variable_warning(event);
}

void EventManagerImpl::handle_event(SDL_TouchFingerEvent& event) noexcept {
	Game_window* gwin = Game_window::get_instance();
	switch (event.type) {
	case SDL_FINGERDOWN:
		if ((!Mouse::use_touch_input) && (event.fingerId != 0)) {
			Mouse::use_touch_input = true;
			gwin->set_painted();
		}
		break;

	case SDL_FINGERUP:
		// Not handled for now. These end up being synthesized into
		// mouse events by SDL anyway.
		break;

	case SDL_FINGERMOTION: {
		SDL_Finger* finger0 = SDL_GetTouchFinger(event.touchId, 0);
		if (finger0 == nullptr) {
			break;
		}
		int numFingers = SDL_GetNumTouchFingers(event.touchId);
		if (numFingers >= 1) {
			invoke_callback(
					fingerMotionCallbacks, numFingers,
					FingerMotion{event.dx, event.dy});
		}
		break;
	}
	default:
		break;
	}
}

void EventManagerImpl::handle_event(SDL_DropEvent& event) noexcept {
	invoke_callback(
			dropFileCallbacks, event.type, reinterpret_cast<uint8*>(event.file),
			MousePosition());
	SDL_free(event.file);
}

void EventManagerImpl::handle_background_event() {
	invoke_callback(appEventCallbacks, AppEvents::OnEnterBackground);
}

void EventManagerImpl::handle_event(SDL_WindowEvent& event) noexcept {
	auto eventID = [&]() {
		switch (event.event) {
		case SDL_WINDOWEVENT_ENTER:
			return WindowEvents::Enter;
		case SDL_WINDOWEVENT_LEAVE:
			return WindowEvents::Leave;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			return WindowEvents::Focus_Gained;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			return WindowEvents::Focus_Lost;
		default:
			return WindowEvents::Unhandled;
		}
	}();
	invoke_callback(windowEventCallbacks, eventID, MousePosition());
}

void EventManagerImpl::handle_quit_event() {}

void EventManagerImpl::handle_event(SDL_Event& event) {
	switch (event.type) {
	case SDL_CONTROLLERDEVICEADDED:
	case SDL_CONTROLLERDEVICEREMOVED:
		handle_event(event.cdevice);
		break;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		handle_event(event.key);
		break;

	case SDL_TEXTINPUT:
		handle_event(event.text);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		handle_event(event.button);
		break;

	case SDL_MOUSEMOTION:
		handle_event(event.motion);
		break;

	case SDL_MOUSEWHEEL:
		handle_event(event.wheel);
		break;

	case SDL_FINGERDOWN:
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
		handle_event(event.tfinger);
		break;

	case SDL_DROPFILE:
		handle_event(event.drop);
		break;

	case SDL_APP_WILLENTERBACKGROUND:
		handle_background_event();
		break;

	case SDL_WINDOWEVENT:
		handle_event(event.window);
		break;

	case SDL_QUIT:
		handle_quit_event();
		break;

	default:
		if (event.type == TouchUI::eventType) {
		} else if (event.type == ShortcutBar_gump::eventType) {
		}
		break;
	}
}

EventManager* EventManager::getInstance() {
	static auto instance(std::make_unique<EventManagerImpl>());
	return instance.get();
}

void EventManagerImpl::handle_events() {
	SDL_Event event;
	while (!invoke_callback(breakLoopCallbacks) && SDL_PollEvent(&event) != 0) {
		handle_event(event);
	}

	handle_gamepad_axis_input();
}

void EventManagerImpl::enable_dropfile() {
#ifdef USE_EXULTSTUDIO
	Game_window*  gwin = Game_window::get_instance();
	SDL_SysWMinfo info;    // Get system info.
	SDL_GetWindowWMInfo(gwin->get_win()->get_screen_window(), &info);
#	ifndef _WIN32
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
#	else
	hgwin = info.info.win.window;
	OleInitialize(nullptr);
	windnd.reset(new Windnd(
			hgwin, Move_dragged_shape, Move_dragged_combo, Drop_dragged_shape,
			Drop_dragged_chunk, Drop_dragged_npc, Drop_dragged_combo));
	if (FAILED(RegisterDragDrop(hgwin, windnd.get()))) {
		windnd.reset();
		std::cout << "Something's wrong with OLE2 ..." << std::endl;
	}
#	endif
#endif
}

void EventManagerImpl::disable_dropfile() {
#ifdef USE_EXULTSTUDIO
#	ifndef _WIN32
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
#	else
	RevokeDragDrop(hgwin);
	windnd.reset();
#	endif
#endif
}
