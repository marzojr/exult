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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "eventman.h"

#include "actors.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "menulist.h"
#include "mouse.h"
#include "touchui.h"

#include <cmath>
#include <iostream>
#include <memory>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wunused-macros"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#include <SDL_error.h>
#include <SDL_events.h>
#ifdef USE_EXULTSTUDIO /* Only needed for communication with exult studio */
#	define Font _XFont_
#	include <SDL_syswm.h>
#	undef Font
#endif
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#if defined(USE_EXULTSTUDIO) && defined(_WIN32)
#	include "windrag.h"
void Move_dragged_shape(
		int shape, int frame, int x, int y, int prevx, int prevy, bool show);
void Move_dragged_combo(
		int xtiles, int ytiles, int tiles_right, int tiles_below, int x, int y,
		int prevx, int prevy, bool show);
void Drop_dragged_shape(int shape, int frame, int x, int y);
void Drop_dragged_chunk(int chunknum, int x, int y);
void Drop_dragged_npc(int npcnum, int x, int y);
void Drop_dragged_combo(int cnt, U7_combo_data* combo, int x, int y);
#endif

namespace {
	template <typename T, detail::require<std::is_floating_point_v<T>> = true>
	[[maybe_unused]] inline bool isAlmostZero(T val) noexcept {
		return std::abs(val) < std::numeric_limits<T>::epsilon();
	}

	template <
			typename T1, typename T2,
			detail::require<
					std::is_floating_point_v<T1>
					&& std::is_floating_point_v<T2>>
			= true>
	[[maybe_unused]] inline bool floatCompare(T1 f1, T2 f2) noexcept {
		using T = std::common_type_t<T1, T2>;
		T v1    = f1;
		T v2    = f2;
		return isAlmostZero(v1 - v2);
	}

	constexpr inline KeyCodes translateKeyCode(uint32 code) noexcept {
		using Tp = std::underlying_type_t<KeyCodes>;
		static_assert(
				static_cast<Tp>(KeyCodes::Key_Unknown) == SDLK_UNKNOWN
				&& static_cast<Tp>(KeyCodes::Key_Return) == SDLK_RETURN
				&& static_cast<Tp>(KeyCodes::Key_Escape) == SDLK_ESCAPE
				&& static_cast<Tp>(KeyCodes::Key_Backspace) == SDLK_BACKSPACE
				&& static_cast<Tp>(KeyCodes::Key_Tab) == SDLK_TAB
				&& static_cast<Tp>(KeyCodes::Key_Space) == SDLK_SPACE
				&& static_cast<Tp>(KeyCodes::Key_Exclaim) == SDLK_EXCLAIM
				&& static_cast<Tp>(KeyCodes::Key_DblQuote) == SDLK_QUOTEDBL
				&& static_cast<Tp>(KeyCodes::Key_Hash) == SDLK_HASH
				&& static_cast<Tp>(KeyCodes::Key_Percent) == SDLK_PERCENT
				&& static_cast<Tp>(KeyCodes::Key_Dollar) == SDLK_DOLLAR
				&& static_cast<Tp>(KeyCodes::Key_Ampersand) == SDLK_AMPERSAND
				&& static_cast<Tp>(KeyCodes::Key_Quote) == SDLK_QUOTE
				&& static_cast<Tp>(KeyCodes::Key_LeftParen) == SDLK_LEFTPAREN
				&& static_cast<Tp>(KeyCodes::Key_RightParen) == SDLK_RIGHTPAREN
				&& static_cast<Tp>(KeyCodes::Key_Asterisk) == SDLK_ASTERISK
				&& static_cast<Tp>(KeyCodes::Key_Plus) == SDLK_PLUS
				&& static_cast<Tp>(KeyCodes::Key_Comma) == SDLK_COMMA
				&& static_cast<Tp>(KeyCodes::Key_Minus) == SDLK_MINUS
				&& static_cast<Tp>(KeyCodes::Key_Period) == SDLK_PERIOD
				&& static_cast<Tp>(KeyCodes::Key_Slash) == SDLK_SLASH
				&& static_cast<Tp>(KeyCodes::Key_0) == SDLK_0
				&& static_cast<Tp>(KeyCodes::Key_1) == SDLK_1
				&& static_cast<Tp>(KeyCodes::Key_2) == SDLK_2
				&& static_cast<Tp>(KeyCodes::Key_3) == SDLK_3
				&& static_cast<Tp>(KeyCodes::Key_4) == SDLK_4
				&& static_cast<Tp>(KeyCodes::Key_5) == SDLK_5
				&& static_cast<Tp>(KeyCodes::Key_6) == SDLK_6
				&& static_cast<Tp>(KeyCodes::Key_7) == SDLK_7
				&& static_cast<Tp>(KeyCodes::Key_8) == SDLK_8
				&& static_cast<Tp>(KeyCodes::Key_9) == SDLK_9
				&& static_cast<Tp>(KeyCodes::Key_Colon) == SDLK_COLON
				&& static_cast<Tp>(KeyCodes::Key_Semicolon) == SDLK_SEMICOLON
				&& static_cast<Tp>(KeyCodes::Key_Less) == SDLK_LESS
				&& static_cast<Tp>(KeyCodes::Key_Equals) == SDLK_EQUALS
				&& static_cast<Tp>(KeyCodes::Key_Greater) == SDLK_GREATER
				&& static_cast<Tp>(KeyCodes::Key_Question) == SDLK_QUESTION
				&& static_cast<Tp>(KeyCodes::Key_At) == SDLK_AT
				&& static_cast<Tp>(KeyCodes::Key_LeftBracket)
						   == SDLK_LEFTBRACKET
				&& static_cast<Tp>(KeyCodes::Key_Backslash) == SDLK_BACKSLASH
				&& static_cast<Tp>(KeyCodes::Key_RightBracket)
						   == SDLK_RIGHTBRACKET
				&& static_cast<Tp>(KeyCodes::Key_Caret) == SDLK_CARET
				&& static_cast<Tp>(KeyCodes::Key_Underscore) == SDLK_UNDERSCORE
				&& static_cast<Tp>(KeyCodes::Key_Backquote) == SDLK_BACKQUOTE
				&& static_cast<Tp>(KeyCodes::Key_a) == SDLK_a
				&& static_cast<Tp>(KeyCodes::Key_b) == SDLK_b
				&& static_cast<Tp>(KeyCodes::Key_c) == SDLK_c
				&& static_cast<Tp>(KeyCodes::Key_d) == SDLK_d
				&& static_cast<Tp>(KeyCodes::Key_e) == SDLK_e
				&& static_cast<Tp>(KeyCodes::Key_f) == SDLK_f
				&& static_cast<Tp>(KeyCodes::Key_g) == SDLK_g
				&& static_cast<Tp>(KeyCodes::Key_h) == SDLK_h
				&& static_cast<Tp>(KeyCodes::Key_i) == SDLK_i
				&& static_cast<Tp>(KeyCodes::Key_j) == SDLK_j
				&& static_cast<Tp>(KeyCodes::Key_k) == SDLK_k
				&& static_cast<Tp>(KeyCodes::Key_l) == SDLK_l
				&& static_cast<Tp>(KeyCodes::Key_m) == SDLK_m
				&& static_cast<Tp>(KeyCodes::Key_n) == SDLK_n
				&& static_cast<Tp>(KeyCodes::Key_o) == SDLK_o
				&& static_cast<Tp>(KeyCodes::Key_p) == SDLK_p
				&& static_cast<Tp>(KeyCodes::Key_q) == SDLK_q
				&& static_cast<Tp>(KeyCodes::Key_r) == SDLK_r
				&& static_cast<Tp>(KeyCodes::Key_s) == SDLK_s
				&& static_cast<Tp>(KeyCodes::Key_t) == SDLK_t
				&& static_cast<Tp>(KeyCodes::Key_u) == SDLK_u
				&& static_cast<Tp>(KeyCodes::Key_v) == SDLK_v
				&& static_cast<Tp>(KeyCodes::Key_w) == SDLK_w
				&& static_cast<Tp>(KeyCodes::Key_x) == SDLK_x
				&& static_cast<Tp>(KeyCodes::Key_y) == SDLK_y
				&& static_cast<Tp>(KeyCodes::Key_z) == SDLK_z
				&& static_cast<Tp>(KeyCodes::Key_CapsLock) == SDLK_CAPSLOCK
				&& static_cast<Tp>(KeyCodes::Key_F1) == SDLK_F1
				&& static_cast<Tp>(KeyCodes::Key_F2) == SDLK_F2
				&& static_cast<Tp>(KeyCodes::Key_F3) == SDLK_F3
				&& static_cast<Tp>(KeyCodes::Key_F4) == SDLK_F4
				&& static_cast<Tp>(KeyCodes::Key_F5) == SDLK_F5
				&& static_cast<Tp>(KeyCodes::Key_F6) == SDLK_F6
				&& static_cast<Tp>(KeyCodes::Key_F7) == SDLK_F7
				&& static_cast<Tp>(KeyCodes::Key_F8) == SDLK_F8
				&& static_cast<Tp>(KeyCodes::Key_F9) == SDLK_F9
				&& static_cast<Tp>(KeyCodes::Key_F10) == SDLK_F10
				&& static_cast<Tp>(KeyCodes::Key_F11) == SDLK_F11
				&& static_cast<Tp>(KeyCodes::Key_F12) == SDLK_F12
				&& static_cast<Tp>(KeyCodes::Key_PrintScreen)
						   == SDLK_PRINTSCREEN
				&& static_cast<Tp>(KeyCodes::Key_ScrollLock) == SDLK_SCROLLLOCK
				&& static_cast<Tp>(KeyCodes::Key_Pause) == SDLK_PAUSE
				&& static_cast<Tp>(KeyCodes::Key_Insert) == SDLK_INSERT
				&& static_cast<Tp>(KeyCodes::Key_Home) == SDLK_HOME
				&& static_cast<Tp>(KeyCodes::Key_PageUp) == SDLK_PAGEUP
				&& static_cast<Tp>(KeyCodes::Key_Delete) == SDLK_DELETE
				&& static_cast<Tp>(KeyCodes::Key_End) == SDLK_END
				&& static_cast<Tp>(KeyCodes::Key_PageDown) == SDLK_PAGEDOWN
				&& static_cast<Tp>(KeyCodes::Key_Right) == SDLK_RIGHT
				&& static_cast<Tp>(KeyCodes::Key_Left) == SDLK_LEFT
				&& static_cast<Tp>(KeyCodes::Key_Down) == SDLK_DOWN
				&& static_cast<Tp>(KeyCodes::Key_Up) == SDLK_UP
				&& static_cast<Tp>(KeyCodes::Key_NumLockClear)
						   == SDLK_NUMLOCKCLEAR
				&& static_cast<Tp>(KeyCodes::Key_KP_Divide) == SDLK_KP_DIVIDE
				&& static_cast<Tp>(KeyCodes::Key_KP_Multiply)
						   == SDLK_KP_MULTIPLY
				&& static_cast<Tp>(KeyCodes::Key_KP_Minus) == SDLK_KP_MINUS
				&& static_cast<Tp>(KeyCodes::Key_KP_Plus) == SDLK_KP_PLUS
				&& static_cast<Tp>(KeyCodes::Key_KP_Enter) == SDLK_KP_ENTER
				&& static_cast<Tp>(KeyCodes::Key_KP_1) == SDLK_KP_1
				&& static_cast<Tp>(KeyCodes::Key_KP_2) == SDLK_KP_2
				&& static_cast<Tp>(KeyCodes::Key_KP_3) == SDLK_KP_3
				&& static_cast<Tp>(KeyCodes::Key_KP_4) == SDLK_KP_4
				&& static_cast<Tp>(KeyCodes::Key_KP_5) == SDLK_KP_5
				&& static_cast<Tp>(KeyCodes::Key_KP_6) == SDLK_KP_6
				&& static_cast<Tp>(KeyCodes::Key_KP_7) == SDLK_KP_7
				&& static_cast<Tp>(KeyCodes::Key_KP_8) == SDLK_KP_8
				&& static_cast<Tp>(KeyCodes::Key_KP_9) == SDLK_KP_9
				&& static_cast<Tp>(KeyCodes::Key_KP_0) == SDLK_KP_0
				&& static_cast<Tp>(KeyCodes::Key_KP_Period) == SDLK_KP_PERIOD
				&& static_cast<Tp>(KeyCodes::Key_Application)
						   == SDLK_APPLICATION
				&& static_cast<Tp>(KeyCodes::Key_Power) == SDLK_POWER
				&& static_cast<Tp>(KeyCodes::Key_KP_Equals) == SDLK_KP_EQUALS
				&& static_cast<Tp>(KeyCodes::Key_F13) == SDLK_F13
				&& static_cast<Tp>(KeyCodes::Key_F14) == SDLK_F14
				&& static_cast<Tp>(KeyCodes::Key_F15) == SDLK_F15
				&& static_cast<Tp>(KeyCodes::Key_F16) == SDLK_F16
				&& static_cast<Tp>(KeyCodes::Key_F17) == SDLK_F17
				&& static_cast<Tp>(KeyCodes::Key_F18) == SDLK_F18
				&& static_cast<Tp>(KeyCodes::Key_F19) == SDLK_F19
				&& static_cast<Tp>(KeyCodes::Key_F20) == SDLK_F20
				&& static_cast<Tp>(KeyCodes::Key_F21) == SDLK_F21
				&& static_cast<Tp>(KeyCodes::Key_F22) == SDLK_F22
				&& static_cast<Tp>(KeyCodes::Key_F23) == SDLK_F23
				&& static_cast<Tp>(KeyCodes::Key_F24) == SDLK_F24
				&& static_cast<Tp>(KeyCodes::Key_Execute) == SDLK_EXECUTE
				&& static_cast<Tp>(KeyCodes::Key_Help) == SDLK_HELP
				&& static_cast<Tp>(KeyCodes::Key_Menu) == SDLK_MENU
				&& static_cast<Tp>(KeyCodes::Key_Select) == SDLK_SELECT
				&& static_cast<Tp>(KeyCodes::Key_Stop) == SDLK_STOP
				&& static_cast<Tp>(KeyCodes::Key_Again) == SDLK_AGAIN
				&& static_cast<Tp>(KeyCodes::Key_Undo) == SDLK_UNDO
				&& static_cast<Tp>(KeyCodes::Key_Cut) == SDLK_CUT
				&& static_cast<Tp>(KeyCodes::Key_Copy) == SDLK_COPY
				&& static_cast<Tp>(KeyCodes::Key_Paste) == SDLK_PASTE
				&& static_cast<Tp>(KeyCodes::Key_Find) == SDLK_FIND
				&& static_cast<Tp>(KeyCodes::Key_Mute) == SDLK_MUTE
				&& static_cast<Tp>(KeyCodes::Key_VolumeUp) == SDLK_VOLUMEUP
				&& static_cast<Tp>(KeyCodes::Key_VolumeDown) == SDLK_VOLUMEDOWN
				&& static_cast<Tp>(KeyCodes::Key_KP_Comma) == SDLK_KP_COMMA
				&& static_cast<Tp>(KeyCodes::Key_KP_EqualsAS400)
						   == SDLK_KP_EQUALSAS400
				&& static_cast<Tp>(KeyCodes::Key_AltErase) == SDLK_ALTERASE
				&& static_cast<Tp>(KeyCodes::Key_SysReq) == SDLK_SYSREQ
				&& static_cast<Tp>(KeyCodes::Key_Cancel) == SDLK_CANCEL
				&& static_cast<Tp>(KeyCodes::Key_Clear) == SDLK_CLEAR
				&& static_cast<Tp>(KeyCodes::Key_Prior) == SDLK_PRIOR
				&& static_cast<Tp>(KeyCodes::Key_Return2) == SDLK_RETURN2
				&& static_cast<Tp>(KeyCodes::Key_Separator) == SDLK_SEPARATOR
				&& static_cast<Tp>(KeyCodes::Key_Out) == SDLK_OUT
				&& static_cast<Tp>(KeyCodes::Key_Oper) == SDLK_OPER
				&& static_cast<Tp>(KeyCodes::Key_ClearAgain) == SDLK_CLEARAGAIN
				&& static_cast<Tp>(KeyCodes::Key_CrSel) == SDLK_CRSEL
				&& static_cast<Tp>(KeyCodes::Key_ExSel) == SDLK_EXSEL
				&& static_cast<Tp>(KeyCodes::Key_KP_00) == SDLK_KP_00
				&& static_cast<Tp>(KeyCodes::Key_KP_000) == SDLK_KP_000
				&& static_cast<Tp>(KeyCodes::Key_ThousandsSeparator)
						   == SDLK_THOUSANDSSEPARATOR
				&& static_cast<Tp>(KeyCodes::Key_DecimalSeparator)
						   == SDLK_DECIMALSEPARATOR
				&& static_cast<Tp>(KeyCodes::Key_CurrencyUnit)
						   == SDLK_CURRENCYUNIT
				&& static_cast<Tp>(KeyCodes::Key_CurrencySubunit)
						   == SDLK_CURRENCYSUBUNIT
				&& static_cast<Tp>(KeyCodes::Key_KP_LeftParen)
						   == SDLK_KP_LEFTPAREN
				&& static_cast<Tp>(KeyCodes::Key_KP_RightParen)
						   == SDLK_KP_RIGHTPAREN
				&& static_cast<Tp>(KeyCodes::Key_KP_LeftBrace)
						   == SDLK_KP_LEFTBRACE
				&& static_cast<Tp>(KeyCodes::Key_KP_RightBrace)
						   == SDLK_KP_RIGHTBRACE
				&& static_cast<Tp>(KeyCodes::Key_KP_Tab) == SDLK_KP_TAB
				&& static_cast<Tp>(KeyCodes::Key_KP_Backspace)
						   == SDLK_KP_BACKSPACE
				&& static_cast<Tp>(KeyCodes::Key_KP_A) == SDLK_KP_A
				&& static_cast<Tp>(KeyCodes::Key_KP_B) == SDLK_KP_B
				&& static_cast<Tp>(KeyCodes::Key_KP_C) == SDLK_KP_C
				&& static_cast<Tp>(KeyCodes::Key_KP_D) == SDLK_KP_D
				&& static_cast<Tp>(KeyCodes::Key_KP_E) == SDLK_KP_E
				&& static_cast<Tp>(KeyCodes::Key_KP_F) == SDLK_KP_F
				&& static_cast<Tp>(KeyCodes::Key_KP_Xor) == SDLK_KP_XOR
				&& static_cast<Tp>(KeyCodes::Key_KP_Power) == SDLK_KP_POWER
				&& static_cast<Tp>(KeyCodes::Key_KP_Percent) == SDLK_KP_PERCENT
				&& static_cast<Tp>(KeyCodes::Key_KP_Less) == SDLK_KP_LESS
				&& static_cast<Tp>(KeyCodes::Key_KP_Greater) == SDLK_KP_GREATER
				&& static_cast<Tp>(KeyCodes::Key_KP_Ampersand)
						   == SDLK_KP_AMPERSAND
				&& static_cast<Tp>(KeyCodes::Key_KP_DblAmpersand)
						   == SDLK_KP_DBLAMPERSAND
				&& static_cast<Tp>(KeyCodes::Key_KP_VerticalBar)
						   == SDLK_KP_VERTICALBAR
				&& static_cast<Tp>(KeyCodes::Key_KP_DblVerticalBar)
						   == SDLK_KP_DBLVERTICALBAR
				&& static_cast<Tp>(KeyCodes::Key_KP_Colon) == SDLK_KP_COLON
				&& static_cast<Tp>(KeyCodes::Key_KP_Hash) == SDLK_KP_HASH
				&& static_cast<Tp>(KeyCodes::Key_KP_Space) == SDLK_KP_SPACE
				&& static_cast<Tp>(KeyCodes::Key_KP_At) == SDLK_KP_AT
				&& static_cast<Tp>(KeyCodes::Key_KP_Exclamation)
						   == SDLK_KP_EXCLAM
				&& static_cast<Tp>(KeyCodes::Key_KP_MemStore)
						   == SDLK_KP_MEMSTORE
				&& static_cast<Tp>(KeyCodes::Key_KP_MemRecall)
						   == SDLK_KP_MEMRECALL
				&& static_cast<Tp>(KeyCodes::Key_KP_MemClear)
						   == SDLK_KP_MEMCLEAR
				&& static_cast<Tp>(KeyCodes::Key_KP_MemAdd) == SDLK_KP_MEMADD
				&& static_cast<Tp>(KeyCodes::Key_KP_MemSubtract)
						   == SDLK_KP_MEMSUBTRACT
				&& static_cast<Tp>(KeyCodes::Key_KP_MemMultiply)
						   == SDLK_KP_MEMMULTIPLY
				&& static_cast<Tp>(KeyCodes::Key_KP_MemDivide)
						   == SDLK_KP_MEMDIVIDE
				&& static_cast<Tp>(KeyCodes::Key_KP_PlusMinus)
						   == SDLK_KP_PLUSMINUS
				&& static_cast<Tp>(KeyCodes::Key_KP_Clear) == SDLK_KP_CLEAR
				&& static_cast<Tp>(KeyCodes::Key_KP_ClearEntry)
						   == SDLK_KP_CLEARENTRY
				&& static_cast<Tp>(KeyCodes::Key_KP_Binary) == SDLK_KP_BINARY
				&& static_cast<Tp>(KeyCodes::Key_KP_Octal) == SDLK_KP_OCTAL
				&& static_cast<Tp>(KeyCodes::Key_KP_Decimal) == SDLK_KP_DECIMAL
				&& static_cast<Tp>(KeyCodes::Key_KP_Hexadecimal)
						   == SDLK_KP_HEXADECIMAL
				&& static_cast<Tp>(KeyCodes::Key_LeftCtrl) == SDLK_LCTRL
				&& static_cast<Tp>(KeyCodes::Key_LeftShift) == SDLK_LSHIFT
				&& static_cast<Tp>(KeyCodes::Key_LeftAlt) == SDLK_LALT
				&& static_cast<Tp>(KeyCodes::Key_LeftGUI) == SDLK_LGUI
				&& static_cast<Tp>(KeyCodes::Key_RightCtrl) == SDLK_RCTRL
				&& static_cast<Tp>(KeyCodes::Key_RightShift) == SDLK_RSHIFT
				&& static_cast<Tp>(KeyCodes::Key_RightAlt) == SDLK_RALT
				&& static_cast<Tp>(KeyCodes::Key_RightGUI) == SDLK_RGUI
				&& static_cast<Tp>(KeyCodes::Key_Mode) == SDLK_MODE
				&& static_cast<Tp>(KeyCodes::Key_AudioNext) == SDLK_AUDIONEXT
				&& static_cast<Tp>(KeyCodes::Key_AudioPrev) == SDLK_AUDIOPREV
				&& static_cast<Tp>(KeyCodes::Key_AudioStop) == SDLK_AUDIOSTOP
				&& static_cast<Tp>(KeyCodes::Key_AudioPlay) == SDLK_AUDIOPLAY
				&& static_cast<Tp>(KeyCodes::Key_AudioMute) == SDLK_AUDIOMUTE
				&& static_cast<Tp>(KeyCodes::Key_MediaSelect)
						   == SDLK_MEDIASELECT
				&& static_cast<Tp>(KeyCodes::Key_WWW) == SDLK_WWW
				&& static_cast<Tp>(KeyCodes::Key_Mail) == SDLK_MAIL
				&& static_cast<Tp>(KeyCodes::Key_Calculator) == SDLK_CALCULATOR
				&& static_cast<Tp>(KeyCodes::Key_Computer) == SDLK_COMPUTER
				&& static_cast<Tp>(KeyCodes::Key_AC_Search) == SDLK_AC_SEARCH
				&& static_cast<Tp>(KeyCodes::Key_AC_Home) == SDLK_AC_HOME
				&& static_cast<Tp>(KeyCodes::Key_AC_Back) == SDLK_AC_BACK
				&& static_cast<Tp>(KeyCodes::Key_AC_Forward) == SDLK_AC_FORWARD
				&& static_cast<Tp>(KeyCodes::Key_AC_Stop) == SDLK_AC_STOP
				&& static_cast<Tp>(KeyCodes::Key_AC_Refresh) == SDLK_AC_REFRESH
				&& static_cast<Tp>(KeyCodes::Key_AC_Bookmarks)
						   == SDLK_AC_BOOKMARKS
				&& static_cast<Tp>(KeyCodes::Key_BrightnessDown)
						   == SDLK_BRIGHTNESSDOWN
				&& static_cast<Tp>(KeyCodes::Key_BrightnessUp)
						   == SDLK_BRIGHTNESSUP
				&& static_cast<Tp>(KeyCodes::Key_DisplaySwitch)
						   == SDLK_DISPLAYSWITCH
				&& static_cast<Tp>(KeyCodes::Key_KbdIllumToggle)
						   == SDLK_KBDILLUMTOGGLE
				&& static_cast<Tp>(KeyCodes::Key_KbdIllumDown)
						   == SDLK_KBDILLUMDOWN
				&& static_cast<Tp>(KeyCodes::Key_KbdIllumUp) == SDLK_KBDILLUMUP
				&& static_cast<Tp>(KeyCodes::Key_Eject) == SDLK_EJECT
				&& static_cast<Tp>(KeyCodes::Key_Sleep) == SDLK_SLEEP
				&& static_cast<Tp>(KeyCodes::Key_App1) == SDLK_APP1
				&& static_cast<Tp>(KeyCodes::Key_App2) == SDLK_APP2
				&& static_cast<Tp>(KeyCodes::Key_AudioRewind)
						   == SDLK_AUDIOREWIND
				&& static_cast<Tp>(KeyCodes::Key_AudioFastforward)
						   == SDLK_AUDIOFASTFORWARD
#if SDL_VERSION_ATLEAST(2, 28, 5)
				&& static_cast<Tp>(KeyCodes::Key_SoftLeft) == SDLK_SOFTLEFT
				&& static_cast<Tp>(KeyCodes::Key_SoftRight) == SDLK_SOFTRIGHT
				&& static_cast<Tp>(KeyCodes::Key_Call) == SDLK_CALL
				&& static_cast<Tp>(KeyCodes::Key_EndCall) == SDLK_ENDCALL
#endif
		);
		return static_cast<KeyCodes>(code);
	}

	constexpr inline KeyMod translateKeyMods(uint32 mods) noexcept {
		using Tp = std::underlying_type_t<KeyMod>;
		static_assert(
				static_cast<Tp>(KeyMod::NoMods) == KMOD_NONE
				&& static_cast<Tp>(KeyMod::LeftShift) == KMOD_LSHIFT
				&& static_cast<Tp>(KeyMod::RightShift) == KMOD_RSHIFT
				&& static_cast<Tp>(KeyMod::LeftCtrl) == KMOD_LCTRL
				&& static_cast<Tp>(KeyMod::RightCtrl) == KMOD_RCTRL
				&& static_cast<Tp>(KeyMod::LeftAlt) == KMOD_LALT
				&& static_cast<Tp>(KeyMod::RightAlt) == KMOD_RALT
				&& static_cast<Tp>(KeyMod::LeftGUI) == KMOD_LGUI
				&& static_cast<Tp>(KeyMod::RightGUI) == KMOD_RGUI
				&& static_cast<Tp>(KeyMod::Num) == KMOD_NUM
				&& static_cast<Tp>(KeyMod::Caps) == KMOD_CAPS
				&& static_cast<Tp>(KeyMod::Mode) == KMOD_MODE
				&& static_cast<Tp>(KeyMod::Scroll) == KMOD_SCROLL
				&& static_cast<Tp>(KeyMod::Ctrl) == KMOD_CTRL
				&& static_cast<Tp>(KeyMod::Shift) == KMOD_SHIFT
				&& static_cast<Tp>(KeyMod::Alt) == KMOD_ALT
				&& static_cast<Tp>(KeyMod::GUI) == KMOD_GUI
				&& static_cast<Tp>(KeyMod::Reserved) == KMOD_RESERVED);
		return static_cast<KeyMod>(mods);
	}

	constexpr inline ControllerButton translateControllerButton(
			uint8 button) noexcept {
		using Tp = std::underlying_type_t<ControllerButton>;
		static_assert(
				(static_cast<Tp>(ControllerButton::Invalid)
				 == SDL_CONTROLLER_BUTTON_INVALID)
				&& (static_cast<Tp>(ControllerButton::A)
					== SDL_CONTROLLER_BUTTON_A)
				&& (static_cast<Tp>(ControllerButton::B)
					== SDL_CONTROLLER_BUTTON_B)
				&& (static_cast<Tp>(ControllerButton::X)
					== SDL_CONTROLLER_BUTTON_X)
				&& (static_cast<Tp>(ControllerButton::Y)
					== SDL_CONTROLLER_BUTTON_Y)
				&& (static_cast<Tp>(ControllerButton::Back)
					== SDL_CONTROLLER_BUTTON_BACK)
				&& (static_cast<Tp>(ControllerButton::Guide)
					== SDL_CONTROLLER_BUTTON_GUIDE)
				&& (static_cast<Tp>(ControllerButton::Start)
					== SDL_CONTROLLER_BUTTON_START)
				&& (static_cast<Tp>(ControllerButton::LeftStick)
					== SDL_CONTROLLER_BUTTON_LEFTSTICK)
				&& (static_cast<Tp>(ControllerButton::RightStick)
					== SDL_CONTROLLER_BUTTON_RIGHTSTICK)
				&& (static_cast<Tp>(ControllerButton::LeftShoulder)
					== SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
				&& (static_cast<Tp>(ControllerButton::RightShoulder)
					== SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
				&& (static_cast<Tp>(ControllerButton::DPad_Up)
					== SDL_CONTROLLER_BUTTON_DPAD_UP)
				&& (static_cast<Tp>(ControllerButton::DPad_Down)
					== SDL_CONTROLLER_BUTTON_DPAD_DOWN)
				&& (static_cast<Tp>(ControllerButton::DPad_Left)
					== SDL_CONTROLLER_BUTTON_DPAD_LEFT)
				&& (static_cast<Tp>(ControllerButton::DPad_Right)
					== SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
				&& (static_cast<Tp>(ControllerButton::Misc1)
					== SDL_CONTROLLER_BUTTON_MISC1)
				&& (static_cast<Tp>(ControllerButton::Paddle1)
					== SDL_CONTROLLER_BUTTON_PADDLE1)
				&& (static_cast<Tp>(ControllerButton::Paddle2)
					== SDL_CONTROLLER_BUTTON_PADDLE2)
				&& (static_cast<Tp>(ControllerButton::Paddle3)
					== SDL_CONTROLLER_BUTTON_PADDLE3)
				&& (static_cast<Tp>(ControllerButton::Paddle4)
					== SDL_CONTROLLER_BUTTON_PADDLE4)
				&& (static_cast<Tp>(ControllerButton::Touchpad)
					== SDL_CONTROLLER_BUTTON_TOUCHPAD));
		return static_cast<ControllerButton>(button);
	}

	constexpr inline MouseButton translateMouseButton(uint8 button) noexcept {
		using Tp = std::underlying_type_t<MouseButton>;
		static_assert(
				(static_cast<Tp>(MouseButton::Left) == SDL_BUTTON_LEFT)
				&& (static_cast<Tp>(MouseButton::Middle) == SDL_BUTTON_MIDDLE)
				&& (static_cast<Tp>(MouseButton::Right) == SDL_BUTTON_RIGHT)
				&& (static_cast<Tp>(MouseButton::X1) == SDL_BUTTON_X1)
				&& (static_cast<Tp>(MouseButton::X2) == SDL_BUTTON_X2));
		return static_cast<MouseButton>(button);
	}

	constexpr inline MouseButtonMask translateMouseMasks(
			uint32 button) noexcept {
		using Tp = std::underlying_type_t<MouseButtonMask>;
		static_assert(
				(static_cast<Tp>(MouseButtonMask::Left) == SDL_BUTTON_LMASK)
				&& (static_cast<Tp>(MouseButtonMask::Middle)
					== SDL_BUTTON_MMASK)
				&& (static_cast<Tp>(MouseButtonMask::Right) == SDL_BUTTON_RMASK)
				&& (static_cast<Tp>(MouseButtonMask::X1) == SDL_BUTTON_X1MASK)
				&& (static_cast<Tp>(MouseButtonMask::X2) == SDL_BUTTON_X2MASK));
		return static_cast<MouseButtonMask>(button);
	}
}    // namespace

bool AxisVector::isNonzero() const noexcept {
	return !isAlmostZero(x) || !isAlmostZero(y);
}

bool AxisTrigger::isNonzero() const noexcept {
	return !isAlmostZero(left) || !isAlmostZero(right);
}

bool FingerMotion::isNonzero() const noexcept {
	return !isAlmostZero(x) || !isAlmostZero(y);
}

MousePosition::MousePosition(get_from_sdl_tag) {
	int x_;
	int y_;
	SDL_GetMouseState(&x_, &y_);
	set(x_, y_);
}

MousePosition::MousePosition(int x_, int y_) {
	set(x_, y_);
}

void MousePosition::set(int x_, int y_) {
	Game_window* gwin = Game_window::get_instance();
	gwin->get_win()->screen_to_game(x_, y_, gwin->get_fastmouse(), x, y);
}

MouseMotion::MouseMotion(int x_, int y_) {
	// Note: maybe not needed?
	Game_window* gwin = Game_window::get_instance();
	gwin->get_win()->screen_to_game(x_, y_, gwin->get_fastmouse(), x, y);
}

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

	void do_mouse_up(MouseButton buttonID);

private:
	inline bool break_event_loop() const;
	inline void handle_event(SDL_Event& event);
	inline void handle_event(SDL_ControllerDeviceEvent& event) noexcept;
	inline void handle_event(SDL_ControllerButtonEvent& event) noexcept;
	inline void handle_event(SDL_KeyboardEvent& event) noexcept;
	inline void handle_event(SDL_TextInputEvent& event) noexcept;
	inline void handle_event(SDL_MouseMotionEvent& event) noexcept;
	inline void handle_event(SDL_MouseButtonEvent& event) noexcept;
	inline void handle_event(SDL_MouseWheelEvent& event) noexcept;
	inline void handle_event(SDL_TouchFingerEvent& event) noexcept;
	inline void handle_event(SDL_DropEvent& event) noexcept;
	inline void handle_event(SDL_WindowEvent& event) noexcept;

	inline void handle_background_event() noexcept;
	inline void handle_quit_event();
	inline void handle_custom_touch_input_event(SDL_UserEvent& event) noexcept;
	inline void handle_custom_mouse_up_event(SDL_UserEvent& event) noexcept;

	inline void handle_gamepad_axis_input() noexcept;

	inline SDL_GameController* find_controller() const noexcept;
	inline SDL_GameController* open_game_controller(
			int joystick_index) const noexcept;

	SDL_GameController* active_gamepad{nullptr};
	uint32      double_click_event_type{std::numeric_limits<uint32>::max()};
	SDL_TimerID double_click_timer_id{0};

	enum {
		INVALID_EVENT    = 0,
		DELAYED_MOUSE_UP = 0x45585545
	};

#if defined(USE_EXULTSTUDIO) && defined(_WIN32)
	WindndPtr windnd{nullptr, OleDeleter{}};
	HWND      hgwin{};
#endif

	template <typename Callback_t, typename... Ts>
	std::invoke_result_t<Callback_t, Ts...> invoke_callback(
			Ts&&... args) const noexcept {
		auto& stack = get_callback_stack<Callback_t>();
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

struct MouseUpData {
	EventManagerImpl* eventMan;
	MouseButton       buttonID;

	MouseUpData(EventManagerImpl* eman, MouseButton button)
			: eventMan(eman), buttonID(button) {}
};

extern "C" uint32 DoMouseUp(uint32 interval, void* param) {
	ignore_unused_variable_warning(interval);
	auto* data = static_cast<MouseUpData*>(param);
	data->eventMan->do_mouse_up(data->buttonID);
	delete data;
	return 0;
}

void EventManagerImpl::do_mouse_up(MouseButton buttonID) {
	SDL_Event event;
	SDL_zero(event);
	event.type      = double_click_event_type;
	event.user.code = DELAYED_MOUSE_UP;
	auto  button    = static_cast<uintptr>(buttonID);
	void* data;
	std::memcpy(&data, &button, sizeof(uintptr));
	event.user.data1 = data;
	event.user.data2 = nullptr;
	SDL_PushEvent(&event);
}

SDL_GameController* EventManagerImpl::open_game_controller(
		int joystick_index) const noexcept {
	SDL_GameController* input_device = SDL_GameControllerOpen(joystick_index);
	if (input_device != nullptr) {
		SDL_GameControllerGetJoystick(input_device);
		std::cout << "Game controller attached and open: \""
				  << SDL_GameControllerName(input_device) << "\"\n";
	} else {
		std::cout
				<< "Game controller attached, but it failed to open. Error: \""
				<< SDL_GetError() << "\"\n";
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
	active_gamepad          = find_controller();
	double_click_event_type = SDL_RegisterEvents(1);
}

bool EventManagerImpl::break_event_loop() const {
	return invoke_callback<BreakLoopCallback>();
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
		if (axis_dead_zone >= std::fabs(value) || isAlmostZero(value)) {
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

	invoke_callback<GamepadAxisCallback>(joy_aim, joy_mouse, joy_rise);
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
					  << "\" detached and closed.\n";
			SDL_GameControllerClose(active_gamepad);
			active_gamepad = find_controller();
		}
		break;
	}
	default:
		break;
	}
}

void EventManagerImpl::handle_event(SDL_ControllerButtonEvent& event) noexcept {
	// TODO: Maybe convert to mouse buttons here?
	const ControllerEvent  kind     = event.type == SDL_CONTROLLERBUTTONDOWN
											  ? ControllerEvent::Pressed
											  : ControllerEvent::Released;
	const ControllerButton buttonID = translateControllerButton(event.button);
	if (buttonID != ControllerButton::Invalid) {
		invoke_callback<ControllerCallback>(kind, buttonID);
	}
}

void EventManagerImpl::handle_event(SDL_KeyboardEvent& event) noexcept {
	const KeyboardEvent kind = event.type == SDL_KEYDOWN
									   ? KeyboardEvent::Pressed
									   : KeyboardEvent::Released;
	const auto          sym  = translateKeyCode(event.keysym.sym);
	const auto          mod  = translateKeyMods(event.keysym.mod);
	invoke_callback<KeyboardCallback>(kind, sym, mod);
}

void EventManagerImpl::handle_event(SDL_TextInputEvent& event) noexcept {
	ignore_unused_variable_warning(event);
	// TODO: This would be a good place to convert input text into the game's
	// codepage. Currently, let's just silently convert non-ASCII characters
	// into '?'.
	char chr = event.text[0];
	if ((chr & 0x80) != 0) {
		chr = '?';
	}
	invoke_callback<TextInputCallback>(chr);
}

void EventManagerImpl::handle_event(SDL_MouseButtonEvent& event) noexcept {
	const MouseButton buttonID = translateMouseButton(event.button);
	if (buttonID != MouseButton::Invalid) {
		const MouseEvent kind   = event.type == SDL_MOUSEBUTTONDOWN
										  ? MouseEvent::Pressed
										  : MouseEvent::Released;
		const uint32     clicks = event.clicks;
		if (kind == MouseEvent::Released && clicks == 1
			&& buttonID != MouseButton::Middle) {
			constexpr static const uint32 delay_ms = 500;
			auto data{std::make_unique<MouseUpData>(this, buttonID)};
			double_click_timer_id
					= SDL_AddTimer(delay_ms, DoMouseUp, data.release());
			return;
		}
		const SDL_bool state
				= kind == MouseEvent::Pressed ? SDL_TRUE : SDL_FALSE;
		Game_window* gwin = Game_window::get_instance();
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), state);
		if (double_click_timer_id == 0) {
			SDL_RemoveTimer(double_click_timer_id);
			double_click_timer_id = 0;
		}
		invoke_callback<MouseButtonCallback>(
				kind, buttonID, clicks, MousePosition(event.x, event.y));
	}
}

void EventManagerImpl::handle_event(SDL_MouseMotionEvent& event) noexcept {
	if (Mouse::use_touch_input && event.which != EXSDL_TOUCH_MOUSEID) {
		Mouse::use_touch_input = false;
	}
	MousePosition mouse(event.x, event.y);
	Mouse::mouse->move(mouse.x, mouse.y);
	Mouse::mouse_update      = true;
	MouseButtonMask buttonID = translateMouseMasks(event.state);
	invoke_callback<MouseMotionCallback>(buttonID, mouse);
}

void EventManagerImpl::handle_event(SDL_MouseWheelEvent& event) noexcept {
	ignore_unused_variable_warning(event);
	MouseMotion delta(event.x, event.y);
	invoke_callback<MouseWheelCallback>(delta);
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
			invoke_callback<FingerMotionCallback>(
					numFingers, FingerMotion{event.dx, event.dy});
		}
		break;
	}
	default:
		break;
	}
}

void EventManagerImpl::handle_event(SDL_DropEvent& event) noexcept {
	invoke_callback<DropFileCallback>(
			event.type, reinterpret_cast<uint8*>(event.file),
			MousePosition(get_from_sdl_tag{}));
	SDL_free(event.file);
}

void EventManagerImpl::handle_background_event() noexcept {
	invoke_callback<AppEventCallback>(AppEvents::OnEnterBackground);
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
	invoke_callback<WindowEventCallback>(
			eventID, MousePosition(get_from_sdl_tag{}));
}

void EventManagerImpl::handle_quit_event() {
	invoke_callback<QuitEventCallback>();
}

void EventManagerImpl::handle_custom_touch_input_event(
		SDL_UserEvent& event) noexcept {
	if (event.code == TouchUI::EVENT_CODE_TEXT_INPUT) {
		const auto* text = static_cast<const char*>(event.data1);
		invoke_callback<TouchInputCallback>(text);
		free(event.data1);
	}
}

void EventManagerImpl::handle_custom_mouse_up_event(
		SDL_UserEvent& event) noexcept {
	if (event.code == DELAYED_MOUSE_UP) {
		uintptr data;
		std::memcpy(&data, &event.data1, sizeof(uintptr));
		const MouseEvent kind     = MouseEvent::Released;
		const uint32     clicks   = 1;
		const auto       buttonID = static_cast<MouseButton>(data);
		Game_window*     gwin     = Game_window::get_instance();
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
		if (double_click_timer_id == 0) {
			SDL_RemoveTimer(double_click_timer_id);
			double_click_timer_id = 0;
		}
		invoke_callback<MouseButtonCallback>(
				kind, buttonID, clicks, MousePosition(get_from_sdl_tag{}));
	}
}

void EventManagerImpl::handle_event(SDL_Event& event) {
	switch (event.type) {
	case SDL_CONTROLLERDEVICEADDED:
	case SDL_CONTROLLERDEVICEREMOVED:
		handle_event(event.cdevice);
		break;

	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		handle_event(event.cbutton);
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
			handle_custom_touch_input_event(event.user);
		} else if (event.type == double_click_event_type) {
			handle_custom_mouse_up_event(event.user);
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
	while (!break_event_loop() && SDL_PollEvent(&event) != 0) {
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
		std::cout << "Something's wrong with OLE2 ...\n";
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

#ifdef DEBUG
namespace {
	[[maybe_unused]] void cb1(
			const AxisVector&, const AxisVector&, const AxisTrigger&) {}

	[[maybe_unused]] void cb2(
			EventManager*, const AxisVector&, const AxisVector&,
			const AxisTrigger&) {}

	struct cb3 {
		[[maybe_unused]] void cb(
				const AxisVector&, const AxisVector&, const AxisTrigger&) {}

		[[maybe_unused]] bool cb2() {
			return false;
		}
	};

	struct cb4 {
		[[maybe_unused]] void operator()(
				const AxisVector&, const AxisVector&, const AxisTrigger&) {}
	};

	[[maybe_unused]] void test_callbacks() {
		auto* events = EventManager::getInstance();
		{ auto guard = events->register_one_callback(cb1); }
		{ auto guard = events->register_one_callback(events, cb2); }
		{
			cb3 vcb3;
			static_assert(detail::has_compatible_callback2_v<
						  decltype(&cb3::cb), cb3*>);
			{ auto guard = events->register_one_callback(&vcb3, &cb3::cb); }
			{
				auto guard
						= events->register_callbacks(vcb3, &cb3::cb, &cb3::cb2);
			}
		}
		{
			cb4 vcb4;
			{
				auto guard = events->register_one_callback(
						&vcb4, &cb4::operator());
			}
			{
				auto guard
						= events->register_one_callback(vcb4, &cb4::operator());
			}
		}
		{
			auto guard = events->register_one_callback(
					+[](const AxisVector&, const AxisVector&,
						const AxisTrigger&) {});
		}
		{
			auto vcb6 = [events](
								const AxisVector&, const AxisVector&,
								const AxisTrigger&) {
				events->enable_dropfile();
			};
			using T6 = std::remove_reference_t<
					std::remove_pointer_t<decltype(vcb6)>>;
			static_assert(!std::is_function_v<T6>);
			static_assert(std::is_object_v<T6>);
			static_assert(detail::has_compatible_callback1_v<T6>);
			static_assert(std::is_invocable_v<
						  T6, const AxisVector&, const AxisVector&,
						  const AxisTrigger&>);
			auto guard = events->register_one_callback(vcb6);
		}
		{
			auto guard = events->register_one_callback(
					[events](
							const AxisVector&, const AxisVector&,
							const AxisTrigger&) {
						events->enable_dropfile();
					});
		}
		{
			auto guard = events->register_callbacks(
					cb1, +[]() {
						return true;
					});
		}
	}
}    // namespace
#endif
