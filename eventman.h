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

#include <functional>
#include <stack>
#include <type_traits>
#include <utility>

// Callback related stuff.

// This is called before every call to SDL_PollEvent. If the callback returns
// true, the event loop is terminated.
using BreakLoopCallback = bool();

// If a gamepad is connected, this callback is called after processing all
// events for the current tick. The game mouse is updated to be in the position
// indicated by 'pos' before the callback.
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

using GamepadAxisCallback
		= void(const AxisVector& leftAxis, const AxisVector& rightAxis,
			   const AxisTrigger& triggers);

// This callback is called when compositing text is finished.
using TextInputCallback = void(char chr);

// This callback is called when mouse buttons are pressed or released.
struct get_from_sdl_tag {};

struct MousePosition {
	int x;
	int y;
	MousePosition() = default;
	MousePosition(get_from_sdl_tag);
	MousePosition(int x_, int y_);
	void set(int x_, int y_);
};

enum class MouseButton {
	Invalid = 0,
	Left    = 1,
	Middle  = 2,
	Right   = 3,
	X1      = 4,
	X2      = 5,
};

enum class MouseButtonMask {
	None   = 0,
	Left   = 1,
	Middle = 2,
	Right  = 4,
	X1     = 8,
	X2     = 16,
};

constexpr inline MouseButtonMask operator|=(
		MouseButtonMask& lhs, MouseButtonMask rhs) {
	using Tp = std::underlying_type_t<MouseButtonMask>;
	lhs      = static_cast<MouseButtonMask>(
            static_cast<Tp>(lhs) | static_cast<Tp>(rhs));
	return lhs;
}

constexpr inline MouseButtonMask operator|(
		MouseButtonMask lhs, MouseButtonMask rhs) {
	lhs |= rhs;
	return lhs;
}

constexpr inline MouseButtonMask operator&=(
		MouseButtonMask& lhs, MouseButtonMask rhs) {
	using Tp = std::underlying_type_t<MouseButtonMask>;
	lhs      = static_cast<MouseButtonMask>(
            static_cast<Tp>(lhs) & static_cast<Tp>(rhs));
	return lhs;
}

constexpr inline MouseButtonMask operator&(
		MouseButtonMask lhs, MouseButtonMask rhs) {
	lhs &= rhs;
	return lhs;
}

enum class MouseEvent {
	Pressed,
	Released,
};

using MouseButtonCallback
		= void(MouseEvent type, MouseButton button, int numClicks,
			   const MousePosition& pos);

// This callback is called when a mouse moves.
using MouseMotionCallback
		= void(MouseButtonMask button, const MousePosition& pos);

// This callback is called on mouse wheel is scrolled.
struct MouseMotion {
	int x;
	int y;
	MouseMotion() = default;
	MouseMotion(int x_, int y_);
};

using MouseWheelCallback = void(MouseMotion& delta);

// This callback is called when a touch-enabled display device sends an event
// for finger(s) moving on the device.
struct FingerMotion {
	float x;
	float y;
	bool  isNonzero() const noexcept;
};

// TODO: Maybe add pressure?
using FingerMotionCallback = void(int numFingers, const FingerMotion& delta);

// This callback is called when any of the monitored events happen.
enum class AppEvents {
	Unhandled,
	OnEnterBackground,    //  The application is about to enter the background
};

using AppEventCallback = void(AppEvents event);

// This callback is called when any of the monitored events happen.
enum class WindowEvents {
	Unhandled,
	Enter,           // Window has gained mouse focus
	Leave,           // Window has lost mouse focus
	Focus_Gained,    // Window has gained keyboard focus
	Focus_Lost,      // Window has lost keyboard focus
};

using WindowEventCallback = void(WindowEvents event, const MousePosition& pos);

// This callback is called whenever a drop from ES happens, if it was enabled by
// a call to EventManager::enable_dropfile.
using DropFileCallback
		= void(uint32 type, const uint8* file, const MousePosition& pos);

// This callback is called when user requests the program to finish (e.g., by
// clicking the 'X' button on the window).
using QuitEventCallback = void();

// This callback is called when the TouchUI custom event happens.
using TouchInputCallback = void(const char* text);

// This callback is called on a delayed click is performed at the shortcut bar.
struct SDL_UserEvent;
using ShortcutBarClickCallback = void(SDL_UserEvent& event);

class EventManager;
using A = std::invoke_result<
		bool (EventManager::*)(), EventManager*, const AxisVector&,
		const AxisVector&, const AxisTrigger&>;

namespace { namespace detail {
	// Some stuff for constraining templates.
	// Cheeky alias for clarity.
	template <bool Condition>
	using require = std::enable_if_t<Condition, bool>;

	// A helper meta-function. I needed this because std::invoke_result is NOT
	// SFINAE-friendly.
	template <typename R, typename F, typename... Ts>
	[[maybe_unused]] constexpr auto apply_invoke_result() {
		if constexpr (std::is_invocable_r_v<R, F, Ts...>) {
			return std::is_same<
					decltype(std::invoke(
							std::declval<F>(), std::declval<Ts>()...)),
					R>{};
		} else {
			return std::false_type{};
		}
	}

	// Some types for checking callback compatibility.
	template <typename C, typename F, typename... Ts>
	struct is_compatible_with : std::false_type {};

	template <typename R, typename... Args, typename F, typename... Ts>
	struct is_compatible_with<R(Args...), F, Ts...>
			: decltype(apply_invoke_result<R, F, Ts..., Args...>()) {};

	template <typename C, typename F, typename... Ts>
	[[maybe_unused]] constexpr inline const bool is_compatible_with_v
			= is_compatible_with<C, F, Ts...>::value;

	template <typename C, typename F, typename... Ts>
	using compatible_with = is_compatible_with<C, F, Ts...>;

	template <typename C, typename F, typename... Ts>
	using compatible_with_t = require<is_compatible_with_v<C, F, Ts...>>;

	// A c++17 version of std::bind_front. Another thing in favor of c++20?
	// From https://stackoverflow.com/a/64714179/5103768
	template <typename T, typename... Args>
	[[maybe_unused]] auto tuple_append(T&& t, Args&&... args) {
		return std::tuple_cat(
				std::forward<T>(t), std::forward_as_tuple(args...));
	}

	template <typename F, typename... FrontArgs>
	[[maybe_unused]] decltype(auto) bind_front(
			F&& callable, FrontArgs&&... args) {
		return [functor   = std::forward<F>(callable),
				frontArgs = std::make_tuple(std::forward<FrontArgs>(args)...)](
					   auto&&... backArgs) {
			return std::apply(
					functor,
					tuple_append(
							frontArgs,
							std::forward<decltype(backArgs)>(backArgs)...));
		};
	}

	// A type list.
	template <typename... Ts>
	struct Type_list {
		constexpr static inline const size_t size = sizeof...(Ts);

		using tail = Type_list<Ts...>;
	};

	template <typename T, typename... Ts>
	struct Type_list<T, Ts...> {
		constexpr static inline const size_t size = sizeof...(Ts) + 1;

		using head = T;
		using tail = Type_list<Ts...>;
	};

	// A meta filter for type lists.
	// From https://codereview.stackexchange.com/a/115774
	template <typename... Lists>
	struct concat;

	template <>
	struct concat<> {
		using type = Type_list<>;
	};

	template <typename... Ts>
	struct concat<Type_list<Ts...>> {
		using type = Type_list<Ts...>;
	};

	template <typename... Ts, typename... Us>
	struct concat<Type_list<Ts...>, Type_list<Us...>> {
		using type = Type_list<Ts..., Us...>;
	};

	template <typename... Ts, typename... Us, typename... Rest>
	struct concat<Type_list<Ts...>, Type_list<Us...>, Rest...> {
		using type = typename concat<Type_list<Ts..., Us...>, Rest...>::type;
	};

	template <typename... Lists>
	struct concat;

	template <template <typename...> typename Pred, typename T>
	using filter_helper
			= std::conditional_t<Pred<T>::value, Type_list<T>, Type_list<>>;

	template <template <typename...> typename Pred, typename... Ts>
	using filter = typename concat<filter_helper<Pred, Ts>...>::type;

	// A custom version of the filter function which passes additional
	// arguments to the predicate.
	template <template <typename...> typename Pred, typename Arg1, typename T>
	using filter_helper1 = std::conditional_t<
			Pred<T, Arg1>::value, Type_list<T>, Type_list<>>;

	template <
			template <typename...> typename Pred, typename Arg1, typename... Ts>
	using filter1 = typename concat<filter_helper1<Pred, Arg1, Ts>...>::type;

	// Underlying data structure for event manager.
	template <typename Callback_t>
	using CallbackStack = std::stack<std::function<Callback_t>>;

	template <
			template <typename...> typename Pred, typename Arg1, typename Arg2,
			typename T>
	using filter_helper2 = std::conditional_t<
			Pred<T, Arg1, Arg2>::value, Type_list<T>, Type_list<>>;

	template <
			template <typename...> typename Pred, typename Arg1, typename Arg2,
			typename... Ts>
	using filter2 =
			typename concat<filter_helper2<Pred, Arg1, Arg2, Ts>...>::type;

	// Underlying data structure for event manager.
	template <typename Callback_t>
	using CallbackStack = std::stack<std::function<Callback_t>>;

	// RAII type for restoring old callback state after registering new
	// callbacks.
	template <typename Callback_t>
	class [[nodiscard]] Callback_guard {
		CallbackStack<Callback_t>* m_target = nullptr;

	public:
		[[nodiscard]] Callback_guard(
				Callback_t&& callback, CallbackStack<Callback_t>& target)
				: m_target(&target) {
			m_target->emplace(std::forward<Callback_t>(callback));
		}

		template <typename Callable, compatible_with_t<Callback_t, Callable>>
		[[nodiscard]] Callback_guard(
				Callable&& callback, CallbackStack<Callback_t>& target)
				: m_target(&target) {
			m_target->emplace(std::forward<Callable>(callback));
		}

		[[nodiscard]] Callback_guard(
				std::function<Callback_t>&& callback,
				CallbackStack<Callback_t>&  target)
				: m_target(&target) {
			m_target->push(std::move(callback));
		}

		~Callback_guard() noexcept {
			if (m_target != nullptr) {
				m_target->pop();
			}
		}

		Callback_guard()                                 = default;
		Callback_guard(const Callback_guard&)            = delete;
		Callback_guard& operator=(const Callback_guard&) = delete;

		Callback_guard(Callback_guard&& other) noexcept
				: m_target(std::exchange(other.m_target, nullptr)) {}

		Callback_guard& operator=(Callback_guard&& other) noexcept {
			m_target = std::exchange(other.m_target, nullptr);
			return *this;
		}
	};

	// Deduction guides for making use of the guard easier.
	template <typename Callback_t>
	Callback_guard(Callback_t*, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	template <typename Callback_t>
	Callback_guard(std::function<Callback_t>&&, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	template <
			typename Callable, typename Callback_t,
			require<std::is_object_v<Callable>> = true>
	Callback_guard(Callable&&, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	// A meta-list with all callback types.
	using Callback_list = detail::Type_list<
			BreakLoopCallback, GamepadAxisCallback, TextInputCallback,
			MouseButtonCallback, MouseMotionCallback, MouseWheelCallback,
			FingerMotionCallback, WindowEventCallback, AppEventCallback,
			DropFileCallback, QuitEventCallback, TouchInputCallback,
			ShortcutBarClickCallback>;

	// Meta function that generates the type of the main data structure of the
	// event manager.
	template <typename... Types>
	[[maybe_unused]] constexpr auto tuple_from_type_list(
			detail::Type_list<Types...>) -> std::tuple<CallbackStack<Types>...>;
	using Callback_tuple_t
			= decltype(tuple_from_type_list(std::declval<Callback_list>()));

	// Meta functions and types for constraining templates.
	template <typename F, typename... Ts>
	[[maybe_unused]] constexpr auto apply_compatible_with_filter1(
			detail::Type_list<Ts...>)
			-> detail::filter1<detail::compatible_with, F, Ts...>;

	template <typename F>
	using has_compatible_callback1 = std::integral_constant<
			bool,
			decltype(apply_compatible_with_filter1<F>(Callback_list{}))::size
					== 1>;

	template <typename F>
	[[maybe_unused]] constexpr inline const bool has_compatible_callback1_v
			= has_compatible_callback1<F>::value;

	template <typename F>
	using get_compatible_callback1 =
			typename decltype(apply_compatible_with_filter1<F>(
					Callback_list{}))::head;

	template <typename F, typename T, typename... Ts>
	[[maybe_unused]] constexpr auto apply_compatible_with_filter2(
			detail::Type_list<Ts...>)
			-> detail::filter2<detail::compatible_with, F, T, Ts...>;

	template <typename F, typename T>
	using has_compatible_callback2 = std::integral_constant<
			bool,
			decltype(apply_compatible_with_filter2<F, T>(Callback_list{}))::size
					== 1>;

	template <typename F, typename T>
	[[maybe_unused]] constexpr inline const bool has_compatible_callback2_v
			= has_compatible_callback2<F, T>::value;

	template <typename F, typename T>
	using get_compatible_callback2 =
			typename decltype(apply_compatible_with_filter2<F, T>(
					Callback_list{}))::head;
}}    // namespace ::detail

class EventManager {
protected:
	template <typename Callback>
	using CallbackStack    = detail::CallbackStack<Callback>;
	using Callback_tuple_t = detail::Callback_tuple_t;
	Callback_tuple_t callbackStacks;

	template <typename Callback>
	CallbackStack<Callback>& get_callback_stack() {
		return std::get<CallbackStack<Callback>>(callbackStacks);
	}

	template <typename Callback>
	const CallbackStack<Callback>& get_callback_stack() const {
		return std::get<CallbackStack<Callback>>(callbackStacks);
	}

	EventManager() = default;

public:
	static EventManager* getInstance();
	virtual ~EventManager()                      = default;
	EventManager(const EventManager&)            = delete;
	EventManager(EventManager&&)                 = delete;
	EventManager& operator=(const EventManager&) = delete;
	EventManager& operator=(EventManager&&)      = delete;

	// Registers one callback (based on type). For function pointers or
	// references.
	template <
			typename Callback,
			detail::require<std::conjunction_v<
					std::is_function<std::remove_reference_t<
							std::remove_pointer_t<Callback>>>,
					detail::has_compatible_callback1<std::remove_reference_t<
							std::remove_pointer_t<Callback>>>>>
			= true>
	[[nodiscard]] auto register_one_callback(Callback&& callback) {
		return detail::Callback_guard(
				std::forward<Callback>(callback),
				get_callback_stack<std::remove_reference_t<
						std::remove_pointer_t<Callback>>>());
	}

	// Registers one callback (based on type). For functors and lambdas.
	template <
			typename Callback,
			detail::require<std::conjunction_v<
					std::is_object<std::remove_reference_t<
							std::remove_pointer_t<Callback>>>,
					detail::has_compatible_callback1<std::remove_reference_t<
							std::remove_pointer_t<Callback>>>>>
			= true>
	[[nodiscard]] auto register_one_callback(Callback&& callback) {
		using Cb = std::remove_reference_t<std::remove_pointer_t<Callback>>;
		using Functor = detail::get_compatible_callback1<Cb>;
		std::function<Functor> fun(std::forward<Callback>(callback));
		return detail::Callback_guard(
				std::move(fun), get_callback_stack<Functor>());
	}

	// Registers one callback (based on type). Accepts various callables, but
	// expects that they need a parameter of type T (pointer or reference) that
	// needs to be passed at the front. Also works with pointer-to-member
	// functions.
	template <
			typename T, typename F,
			detail::require<detail::has_compatible_callback2_v<F, T>> = true>
	[[nodiscard]] auto register_one_callback(T&& data, F&& callback) {
		using Callback = detail::get_compatible_callback2<F, T>;
		std::function<Callback> fun
				= detail::bind_front(std::forward<F>(callback), data);
		return detail::Callback_guard(
				std::move(fun), get_callback_stack<Callback>());
	}

	// Registers many callbacks (based on type). Forwards each parameter to its
	// own call to register_one_callback, then aggregates the results.
	template <
			typename... Fs,
			detail::require<std::conjunction_v<std::is_invocable<
					decltype(&EventManager::register_one_callback<Fs>),
					std::add_pointer_t<EventManager>, Fs>...>>
			= true>
	[[nodiscard]] auto register_callbacks(Fs&&... callback) {
		static_assert(sizeof...(Fs) != 0);
		if constexpr (sizeof...(Fs) == 1) {
			return register_one_callback(std::forward<Fs...>(callback...));
		} else {
			return std::make_tuple(
					register_one_callback(std::forward<Fs>(callback))...);
		}
	}

	// Registers many callbacks (based on type). Splits off the first argument,
	// then forwards it along with each of the remaining parameter to their own
	// calls to register_one_callback, then aggregates the results.

	template <
			typename T, typename... Fs,
			detail::require<std::conjunction_v<std::is_invocable<
					decltype(&EventManager::register_one_callback<T, Fs>),
					std::add_pointer_t<EventManager>, T, Fs>...>>
			= true>
	[[nodiscard]] auto register_callbacks(T&& data, Fs&&... callback) {
		static_assert(sizeof...(Fs) != 0);
		if constexpr (sizeof...(Fs) == 1) {
			return register_one_callback(
					data, std::forward<Fs...>(callback...));
		} else {
			return std::make_tuple(
					register_one_callback(data, std::forward<Fs>(callback))...);
		}
	}

	virtual void handle_events()    = 0;
	virtual void enable_dropfile()  = 0;
	virtual void disable_dropfile() = 0;
};

#endif    // INPUT_MANAGER_H
