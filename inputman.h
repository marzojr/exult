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

#include <stack>
#include <type_traits>
#include <utility>

union SDL_Event;
struct SDL_ControllerAxisEvent;

struct AxisVector {
	float x;
	float y;
	bool isNonzero() const noexcept;
};

enum class AxisEventKind {
	GamepadLeftAxis,
	GamepadRightAxis,
	GamepadTrigger,
};

class InputManager {
public:
	template <typename Callback_t>
	struct Callback_wrapper {
		void*      data;
		Callback_t callback;
	};

	template <typename Callback_t>
	using CallbackStack = std::stack<Callback_wrapper<Callback_t>>;

	template <typename Callback_t>
	struct Callback_guard {
		Callback_guard(
				void* data, Callback_t callback,
				CallbackStack<Callback_t>& target)
				: m_target(target) {
			m_target.push({data, callback});
		}

		~Callback_guard() noexcept {
			m_target.pop();
		}

		Callback_guard(const Callback_guard&)            = delete;
		Callback_guard(Callback_guard&&)                 = delete;
		Callback_guard& operator=(const Callback_guard&) = delete;
		Callback_guard& operator=(Callback_guard&&)      = delete;

	private:
		CallbackStack<Callback_t>& m_target;
	};

	template <typename Callback_t>
	Callback_guard(void* data, Callback_t, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	bool break_event_loop() const;
	void handle_event(SDL_Event& event) noexcept;
	void handle_events() noexcept;

	using GamepadAxisCallback
			= void (*)(void*, AxisEventKind, const AxisVector&);

	[[nodiscard]] auto register_callback(
			void* data, GamepadAxisCallback callback) {
		return Callback_guard(data, callback, gamepadAxisCallbacks);
	}

	using BreakLoopCallback = bool (*)(void*);

	[[nodiscard]] auto register_callback(
			void* data, BreakLoopCallback callback) {
		return Callback_guard(data, callback, breakLoopCallbacks);
	}

private:
	void handle_axis_motion(SDL_ControllerAxisEvent& event) noexcept;

	AxisVector joy_aim{};
	AxisVector joy_mouse{};
	AxisVector joy_rise{};

	CallbackStack<GamepadAxisCallback> gamepadAxisCallbacks;
	CallbackStack<BreakLoopCallback>   breakLoopCallbacks;

	template <typename Callback_t, typename... Ts>
	auto invoke_callback(const CallbackStack<Callback_t>& stack, Ts&&... args)
			const noexcept {
		if (!stack.empty()) {
			const auto& [data, callback] = stack.top();
			return callback(data, std::forward<Ts>(args)...);
		}
		using Result = decltype((std::declval<Callback_t>())(
				nullptr, std::forward<Ts>(args)...));
		if constexpr (!std::is_same_v<void, std::decay_t<Result>>) {
			return Result{};
		}
	}
};

#endif    // INPUT_MANAGER_H
