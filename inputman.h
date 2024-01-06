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
	bool  isNonzero() const noexcept;
};

enum class AxisEventKind {
	GamepadLeftAxis,
	GamepadRightAxis,
	GamepadTrigger,
};

namespace { namespace detail {
	template <typename C, typename F, typename T>
	struct is_compatible_with {};

	template <class R, class... Args, typename F, typename T>
	struct is_compatible_with<R(Args...), F, T*>
			: std::bool_constant<
					  std::is_invocable_r_v<R, F, T*, Args...>
					  && std::is_same_v<
							  R, std::invoke_result_t<F, T*, Args...>>> {};

	template <class R, class... Args, typename F, typename T>
	struct is_compatible_with<R(Args...), F, T>
			: std::bool_constant<
					  std::is_invocable_r_v<R, F, T*, Args...>
					  && std::is_same_v<
							  R, std::invoke_result_t<F, T*, Args...>>> {};

	template <class C, typename F, typename T>
	[[maybe_unused]] constexpr inline const bool is_compatible_with_v
			= is_compatible_with<C, F, T>::value;

	template <class C, typename F, typename T>
	using compatible_with_t
			= std::enable_if_t<is_compatible_with_v<C, F, T>, bool>;

}}    // namespace ::detail

class InputManager {
public:
	InputManager();

	template <typename Callback_t>
	using CallbackStack = std::stack<std::function<Callback_t>>;

private:
	template <typename Callback_t>
	struct Callback_guard {
		Callback_guard(Callback_t callback, CallbackStack<Callback_t>& target)
				: m_target(target) {
			m_target.push(callback);
		}

		Callback_guard(
				std::function<Callback_t>&& callback,
				CallbackStack<Callback_t>&  target)
				: m_target(target) {
			m_target.push(std::move(callback));
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
	[[maybe_unused]] Callback_guard(Callback_t*, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	template <typename Callback_t>
	[[maybe_unused]] Callback_guard(
			std::function<Callback_t>&&, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

public:
	using GamepadAxisCallback = void(AxisEventKind, const AxisVector&);

	[[nodiscard]] auto register_callback(GamepadAxisCallback callback) {
		return Callback_guard(callback, gamepadAxisCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<GamepadAxisCallback, F, T> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<GamepadAxisCallback> fun
				= std::bind(callback, data, _1, _2);
		return Callback_guard(std::move(fun), gamepadAxisCallbacks);
	}

	using BreakLoopCallback = bool();

	[[nodiscard]] auto register_callback(BreakLoopCallback callback) {
		return Callback_guard(callback, breakLoopCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<BreakLoopCallback, F, T> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<BreakLoopCallback> fun
				= std::bind(callback, data, _1, _2);
		return Callback_guard(std::move(fun), breakLoopCallbacks);
	}

private:
	bool break_event_loop() const;
	void handle_event(SDL_Event& event) noexcept;
	void handle_events() noexcept;

	void handle_axis_motion(SDL_ControllerAxisEvent& event) noexcept;

	AxisVector joy_aim{};
	AxisVector joy_mouse{};
	AxisVector joy_rise{};

	CallbackStack<GamepadAxisCallback> gamepadAxisCallbacks;
	CallbackStack<BreakLoopCallback>   breakLoopCallbacks;

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

#endif    // INPUT_MANAGER_H
