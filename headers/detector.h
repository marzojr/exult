/*
 *  Copyright (C) 2020 Marzo Sette Torres Junior
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
#ifndef DETECTOR_H
#define DETECTOR_H

#include <type_traits>

// Working around this clang bug with pragma push/pop:
// https://github.com/clangd/clangd/issues/1167
static_assert(true);

#ifdef __clang__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-macros"
#	pragma GCC diagnostic ignored "-Wunused-template"
#endif

namespace detail {
	// is_detected and related (ported from Library Fundamentals v2).
	namespace inner {
		template <
				typename Default, typename AlwaysVoid,
				template <typename...> class Op, typename... Args>
		struct detector {
			using value_t = std::false_type;
			using type    = Default;
		};

		template <
				typename Default, template <typename...> class Op,
				typename... Args>
		struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
			using value_t = std::true_type;
			using type    = Op<Args...>;
		};

		struct nonesuch final {
			nonesuch(const nonesuch&)                    = delete;
			nonesuch(nonesuch&&)                         = delete;
			nonesuch()                                   = delete;
			~nonesuch()                                  = delete;
			auto operator=(const nonesuch&) -> nonesuch& = delete;
			auto operator=(nonesuch&&) -> nonesuch&      = delete;
		};
	}    // namespace inner

	template <template <typename...> class Op, typename... Args>
	using is_detected = typename inner::detector<
			inner::nonesuch, void, Op, Args...>::value_t;

	template <template <typename...> class Op, typename... Args>
	constexpr bool is_detected_v = is_detected<Op, Args...>::value;

	template <template <typename...> class Op, typename... Args>
	using detected_t =
			typename inner::detector<inner::nonesuch, void, Op, Args...>::type;

	template <
			typename Default, template <typename...> class Op, typename... Args>
	using detected_or = inner::detector<Default, void, Op, Args...>;

	template <
			typename Default, template <typename...> class Op, typename... Args>
	using detected_or_t =
			typename inner::detector<Default, void, Op, Args...>::type;

	template <
			typename Expected, template <typename...> class Op,
			typename... Args>
	constexpr bool is_detected_exact_v
			= std::is_same_v<Expected, detected_t<Op, Args...>>;

	template <
			typename Expected, template <typename...> class Op,
			typename... Args>
	using is_detected_exact
			= std::bool_constant<is_detected_exact_v<Expected, Op, Args...>>;

	template <typename To, template <typename...> class Op, typename... Args>
	constexpr bool is_detected_convertible_v
			= std::is_convertible_v<detected_t<Op, Args...>, To>;

	template <typename To, template <typename...> class Op, typename... Args>
	using is_detected_convertible
			= std::bool_constant<is_detected_convertible_v<To, Op, Args...>>;

	template <template <typename...> class Op, typename... Args>
	constexpr bool exists = is_detected_v<Op, Args...>;

	template <typename T, template <typename...> class Op, typename... Args>
	constexpr bool exists_as = is_detected_exact_v<T, Op, Args...>;
}    // namespace detail

#ifdef __clang__
#	pragma GCC diagnostic pop
#endif

#endif    // DETECTOR_H
