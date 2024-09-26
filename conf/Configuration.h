/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "XMLEntity.h"
#include "detector.h"
#include "istring.h"

#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace detail {
	namespace functions {
		template <typename T>
		using to_string = decltype(to_string(std::declval<T>()));

		template <typename T>
		using from_string = decltype(from_string(
				std::declval<std::string_view>(), std::declval<T>()));
	}    // namespace functions

	// Robust SFINAE idiom by Jonathan Boccara
	// https://www.fluentcpp.com/2019/08/23/how-to-make-sfinae-pretty-and-robust/
	template <typename T>
	constexpr bool has_to_string = exists<functions::to_string, T>;

	template <typename T>
	using has_to_string_t = std::enable_if_t<has_to_string<T>, bool>;

	template <typename T>
	constexpr bool has_from_string = exists_as<T, functions::from_string, T>;

	template <typename T>
	using has_from_string_t = std::enable_if_t<has_from_string<T>, bool>;

	template <
			typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = false>
	Enum from_string(std::string_view s, Enum defaultValue) {
		using underlying_type = std::underlying_type_t<Enum>;
		underlying_type i;
		const auto*     start = s.data();
		const auto*     end   = std::next(start, s.size());
		auto [p, ec]          = std::from_chars(start, end, i);
		if (ec != std::errc() || p != end) {
			return defaultValue;
		}
		return static_cast<Enum>(i);
	}
}    // namespace detail

class Configuration {
public:
	Configuration()
			: xmltree(std::make_unique<XMLnode>("config")), rootname("config") {
	}

	Configuration(const std::string& fname, std::string_view root)
			: xmltree(std::make_unique<XMLnode>(root)), rootname(root) {
		if (!fname.empty()) {
			read_config_file(fname);
		}
	}

	bool read_config_file(
			const std::string& input_filename, std::string_view root = "");
	bool read_abs_config_file(
			std::string_view input_filename, std::string_view root = "");

	bool read_config_string(std::string_view s);

	void value(
			std::string_view key, std::string& ret,
			std::string_view defaultvalue = "") const;

	void value(
			std::string_view key, bool& ret, bool defaultvalue = false) const;

	void value(std::string_view key, int& ret, int defaultvalue = 0) const;
	void value(
			std::string_view key, double& ret, double defaultvalue = 0) const;

	// Don't want implicit conversions to bool.
	template <
			typename Bool,
			std::enable_if_t<std::is_same_v<Bool, bool>, bool> = false>
	void value(
			std::string_view key, Bool& ret, Bool default_value = false) const {
		using std::string_view_literals::operator""sv;
		std::string_view defaultValueStr = default_value ? "yes"sv : "no"sv;

		std::string keyValue;
		value(key, keyValue, defaultValueStr);

		if (!default_value) {
			ret = Pentagram::iequals(keyValue, "yes");
		} else {
			ret = !Pentagram::iequals(keyValue, "no");
		}
	}

	template <
			typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = false>
	void value(std::string_view key, Enum& ret, Enum default_value = {}) const {
		using std::to_string;    // For ADL.
		const std::string defaultValueStr{to_string(default_value)};

		std::string keyValue;
		value(key, keyValue, defaultValueStr);

		using detail::from_string;
		ret = from_string(std::string_view(keyValue), default_value);
	}

	bool key_exists(std::string_view key) const;

	void set(std::string_view key, std::string_view value, bool write_out);

	template <
			typename Int,
			std::enable_if_t<std::is_integral_v<Int>, bool> = false>
	void set(std::string_view key, Int value, bool write_out) {
		if constexpr (std::is_same_v<Int, bool>) {
			using std::string_view_literals::operator""sv;
			std::string_view valueStr = value ? "yes"sv : "no"sv;
			set(key, valueStr, write_out);
		} else {
			std::array<char, std::numeric_limits<Int>::digits10 + 3> buffer;
			const auto* start = buffer.data();
			const auto* end   = std::next(start, value.size());
			auto [p, ec]      = std::to_chars(start, end, value);
			if (ec != std::errc()) {
				value = 0;
				assert(ec == std::errc());
			}
			std::fill(p, end, '\0');    // Just in case.
			set(key, std::string_view(buffer.data(), p - start), write_out);
		}
	}

	template <
			typename Float,
			std::enable_if_t<std::is_floating_point_v<Float>, bool> = false>
	void set(std::string_view key, Float value, bool write_out) {
		std::array<
				char, std::numeric_limits<Float>::max_digits10
							  + std::numeric_limits<Float>::max_exponent10 + 5>
					buffer;
		const auto* start = buffer.data();
		const auto* end   = std::next(start, value.size());
		auto [p, ec]
				= std::to_chars(start, end, value, std::chars_format::fixed);
		if (ec != std::errc()) {
			value = std::numeric_limits<double>::quiet_NaN();
			assert(ec == std::errc());
		}
		std::fill(p, end, '\0');    // Just in case.
		set(key, std::string_view(buffer.data(), p - start), write_out);
	}

	// Don't want implicit conversions to bool.
	template <
			typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = false>
	void set(std::string_view key, Enum value, bool write_out) {
		if constexpr (!detail::exists<detail::functions::to_string, Enum>) {
			return;
		}
		using std::to_string;    // For ADL.
		set(key, to_string(value), write_out);
	}

	void remove(std::string_view key, bool write_out);

	// Return a list of keys that are subsidiary to the supplied key
	std::vector<std::string> listkeys(
			std::string_view key, bool longformat = true) const;

	std::string   dump();    // Assembles a readable representation
	std::ostream& dump(std::ostream& o, std::string_view indentstr);

	void write_back();

	void clear(std::string_view new_root = "");

	using KeyType     = XMLnode::KeyType;
	using KeyTypeList = XMLnode::KeyTypeList;

	void getsubkeys(KeyTypeList& ktl, std::string_view basekey);

private:
	std::unique_ptr<XMLnode> xmltree;
	std::string              rootname;
	std::string              filename;
	bool                     is_file = false;
};

// Global Config
extern Configuration* config;

#endif
