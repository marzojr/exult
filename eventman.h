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

// This callback is called whenever a drop from ES happens, if it was enabled by
// a call to EventManager::enable_dropfile.
using TouchInputCallback = void(const char* text);

// This callback is called on a delayed click is performed at the shortcutbar.
struct SDL_UserEvent;
using ShortcutBarClickCallback = void(SDL_UserEvent& event);

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

	[[nodiscard]] auto register_callback(BreakLoopCallback callback) {
		return Callback_guard(callback, breakLoopCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<BreakLoopCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<BreakLoopCallback> fun = std::bind(callback, data);
		return Callback_guard(std::move(fun), breakLoopCallbacks);
	}

	[[nodiscard]] auto register_callback(GamepadAxisCallback callback) {
		return Callback_guard(callback, gamepadAxisCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<GamepadAxisCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<GamepadAxisCallback> fun
				= std::bind(callback, data, _1, _2, _3);
		return Callback_guard(std::move(fun), gamepadAxisCallbacks);
	}

	[[nodiscard]] auto register_callback(MouseButtonCallback callback) {
		return Callback_guard(callback, mouseButtonCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<MouseButtonCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<MouseButtonCallback> fun
				= std::bind(callback, data, _1, _2, _3, _4);
		return Callback_guard(std::move(fun), mouseButtonCallbacks);
	}

	[[nodiscard]] auto register_callback(MouseMotionCallback callback) {
		return Callback_guard(callback, mouseMotionCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<MouseMotionCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<MouseMotionCallback> fun
				= std::bind(callback, data, _1, _2);
		return Callback_guard(std::move(fun), mouseMotionCallbacks);
	}

	[[nodiscard]] auto register_callback(MouseWheelCallback callback) {
		return Callback_guard(callback, mouseWheelCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<MouseWheelCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<MouseWheelCallback> fun = std::bind(callback, data, _1);
		return Callback_guard(std::move(fun), mouseWheelCallbacks);
	}

	[[nodiscard]] auto register_callback(FingerMotionCallback callback) {
		return Callback_guard(callback, fingerMotionCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<FingerMotionCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<FingerMotionCallback> fun
				= std::bind(callback, data, _1, _2);
		return Callback_guard(std::move(fun), fingerMotionCallbacks);
	}

	[[nodiscard]] auto register_callback(AppEventCallback callback) {
		return Callback_guard(callback, appEventCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<AppEventCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<AppEventCallback> fun = std::bind(callback, data, _1);
		return Callback_guard(std::move(fun), appEventCallbacks);
	}

	[[nodiscard]] auto register_callback(WindowEventCallback callback) {
		return Callback_guard(callback, windowEventCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<WindowEventCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<WindowEventCallback> fun
				= std::bind(callback, data, _1, _2);
		return Callback_guard(std::move(fun), windowEventCallbacks);
	}

	[[nodiscard]] auto register_callback(DropFileCallback callback) {
		return Callback_guard(callback, dropFileCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<DropFileCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<DropFileCallback> fun
				= std::bind(callback, data, _1, _2, _3);
		return Callback_guard(std::move(fun), dropFileCallbacks);
	}

	[[nodiscard]] auto register_callback(QuitEventCallback callback) {
		return Callback_guard(callback, quitEventCallback);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<QuitEventCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<QuitEventCallback> fun = std::bind(callback, data, _1);
		return Callback_guard(std::move(fun), quitEventCallback);
	}

	[[nodiscard]] auto register_callback(TouchInputCallback callback) {
		return Callback_guard(callback, touchInputCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<TouchInputCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<TouchInputCallback> fun = std::bind(callback, data, _1);
		return Callback_guard(std::move(fun), touchInputCallbacks);
	}

	[[nodiscard]] auto register_callback(ShortcutBarClickCallback callback) {
		return Callback_guard(callback, shortcutBarClickCallbacks);
	}

	template <
			typename F, typename T,
			detail::compatible_with_t<ShortcutBarClickCallback, F, T*> = true>
	[[nodiscard]] auto register_callback(F callback, T* data) {
		using namespace std::placeholders;
		std::function<ShortcutBarClickCallback> fun
				= std::bind(callback, data, _1);
		return Callback_guard(std::move(fun), shortcutBarClickCallbacks);
	}

	virtual void handle_events()    = 0;
	virtual void enable_dropfile()  = 0;
	virtual void disable_dropfile() = 0;

protected:
	EventManager();

	CallbackStack<BreakLoopCallback>        breakLoopCallbacks;
	CallbackStack<GamepadAxisCallback>      gamepadAxisCallbacks;
	CallbackStack<MouseButtonCallback>      mouseButtonCallbacks;
	CallbackStack<MouseMotionCallback>      mouseMotionCallbacks;
	CallbackStack<MouseWheelCallback>       mouseWheelCallbacks;
	CallbackStack<FingerMotionCallback>     fingerMotionCallbacks;
	CallbackStack<WindowEventCallback>      windowEventCallbacks;
	CallbackStack<AppEventCallback>         appEventCallbacks;
	CallbackStack<DropFileCallback>         dropFileCallbacks;
	CallbackStack<QuitEventCallback>        quitEventCallback;
	CallbackStack<TouchInputCallback>       touchInputCallbacks;
	CallbackStack<ShortcutBarClickCallback> shortcutBarClickCallbacks;
};

#endif    // INPUT_MANAGER_H
