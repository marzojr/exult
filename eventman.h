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

#include <common_types.h>

#include <stack>
#include <type_traits>
#include <utility>

// Callback related stuff.
struct AxisVector {
	float x;
	float y;
	bool  isNonzero() const noexcept;
};

struct AxisTrigger {
	float left;
	float right;
	bool  isNonzero() const noexcept;
};

struct MousePosition {
	int x;
	int y;
	MousePosition();
	MousePosition(int x_, int y_);
};

using GamepadAxisCallback
		= void(const AxisVector& leftAxis, const AxisVector& rightAxis,
			   const AxisTrigger& triggers);
using BreakLoopCallback = bool();
using DropFileCallback
		= void(uint32 type, const uint8* file, const MousePosition& loc);

namespace { namespace detail {
	template <typename C, typename F, typename... Ts>
	struct is_compatible_with {};

	template <typename R, typename... Args, typename F, typename... Ts>
	struct is_compatible_with<R(Args...), F, Ts...>
			: std::bool_constant<
					  std::is_invocable_r_v<R, F, Ts..., Args...>
					  && std::is_same_v<
							  R, std::invoke_result_t<F, Ts..., Args...>>> {};

	template <typename C, typename F, typename... Ts>
	[[maybe_unused]] constexpr inline const bool is_compatible_with_v
			= is_compatible_with<C, F, Ts...>::value;

	template <typename C, typename F, typename... Ts>
	using compatible_with_t
			= std::enable_if_t<is_compatible_with_v<C, F, Ts...>, bool>;

}}    // namespace ::detail

template <typename Callback_t>
using CallbackStack = std::stack<std::function<Callback_t>>;

template <typename Callback_t>
class [[nodiscard]] Callback_guard {
	friend class EventManager;

	[[nodiscard]] Callback_guard(
			Callback_t&& callback, CallbackStack<Callback_t>& target)
			: m_target(target) {
		m_target.emplace(std::forward<Callback_t>(callback));
	}

	template <
			typename Callable, detail::compatible_with_t<Callback_t, Callable>>
	[[nodiscard]] Callback_guard(
			Callable&& callback, CallbackStack<Callback_t>& target)
			: m_target(target) {
		m_target.emplace(std::forward<Callable>(callback));
	}

	[[nodiscard]] Callback_guard(
			std::function<Callback_t>&& callback,
			CallbackStack<Callback_t>&  target)
			: m_target(target) {
		m_target.push(std::move(callback));
	}

	~Callback_guard() noexcept {
		m_target.pop();
	}

	CallbackStack<Callback_t>& m_target;

public:
	Callback_guard(const Callback_guard&)            = delete;
	Callback_guard(Callback_guard&&)                 = delete;
	Callback_guard& operator=(const Callback_guard&) = delete;
	Callback_guard& operator=(Callback_guard&&)      = delete;
};

template <typename Callback_t>
[[maybe_unused]] Callback_guard(Callback_t*, CallbackStack<Callback_t>&)
		-> Callback_guard<Callback_t>;

template <typename Callback_t>
[[maybe_unused]] Callback_guard(
		std::function<Callback_t>&&, CallbackStack<Callback_t>&)
		-> Callback_guard<Callback_t>;

template <
		typename Callable, typename Callback_t,
		std::enable_if_t<std::is_object_v<Callable>, bool> = true>
[[maybe_unused]] Callback_guard(Callable&&, CallbackStack<Callback_t>&)
		-> Callback_guard<Callback_t>;

class EventManager {
public:
	EventManager* getInstance();
	virtual ~EventManager();
	EventManager(const EventManager&)            = delete;
	EventManager(EventManager&&)                 = delete;
	EventManager& operator=(const EventManager&) = delete;
	EventManager& operator=(EventManager&&)      = delete;

	[[nodiscard]] auto register_callback(GamepadAxisCallback callback) {
		return Callback_guard(callback, gamepadAxisCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<GamepadAxisCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<GamepadAxisCallback> fun
				= std::bind(callback, data, _1, _2, _3, _4);
		return Callback_guard(std::move(fun), gamepadAxisCallbacks);
	}

	[[nodiscard]] auto register_callback(BreakLoopCallback callback) {
		return Callback_guard(callback, breakLoopCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<BreakLoopCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<BreakLoopCallback> fun
				= std::bind(callback, data, _1, _2);
		return Callback_guard(std::move(fun), breakLoopCallbacks);
	}

	[[nodiscard]] auto register_callback(DropFileCallback callback) {
		return Callback_guard(callback, dropFileCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<DropFileCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<DropFileCallback> fun = std::bind(callback, data, _1, _2);
		return Callback_guard(std::move(fun), dropFileCallbacks);
	}

	virtual void handle_events()    = 0;
	virtual void enable_dropfile()  = 0;
	virtual void disable_dropfile() = 0;

protected:
	EventManager();

	CallbackStack<GamepadAxisCallback> gamepadAxisCallbacks;
	CallbackStack<BreakLoopCallback>   breakLoopCallbacks;
	CallbackStack<DropFileCallback>    dropFileCallbacks;
};

#endif    // INPUT_MANAGER_H
