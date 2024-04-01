/*
Copyright (C) 2001-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "listfiles.h"

#include "utils.h"

#include <cstring>
#include <iostream>
#include <string>

// Need iOS >= 13 and NDK >= 22 for std::filesystem
#if __has_include(<filesystem>) && !defined(__IPHONEOS__) && !defined(ANDROID)
#	include <filesystem>
#	include <regex>
#	define USE_STD_FILESYSTEM 1
#elif __has_include(<glob.h>)
#	include <glob.h>
#else
#	error "No way to list files: neither std::filesystem nor glob are available."
#endif

#ifdef ANDROID
#	include <SDL_system.h>
#endif

using std::string;

// TODO: If SDL ever adds directory traversal to rwops, update U7ListFiles() to
// use it.

#ifdef USE_STD_FILESYSTEM
// We have std::filesystem.

static int U7ListFilesImp(
		const std::string& directory, const std::string& mask,
		FileList& files) {
	namespace fs = std::filesystem;
	fs::path source(directory);
	if (!fs::exists(source)) {
		std::cerr << "Directory does not exist: " << directory << std::endl;
		return ENOENT;
	}
	std::regex re = [&mask]() {
		std::string regex_mask;
		regex_mask.reserve(mask.size() + 10);
		for (char c : mask) {
			switch (c) {
			case '*':
				regex_mask += ".*";
				break;
			case '?':
				regex_mask += '.';
				break;
			case '.':
				regex_mask += "\\.";
				break;
			default:
				regex_mask += c;
				break;
			}
		}
		return std::regex(
				regex_mask, std::regex::ECMAScript | std::regex::icase);
	}();
	std::error_code ec{};
	// In here, we convert the [path] directly to a std::string_view if it was a
	// std::string, or we reinterpret the data as a char* if it was a
	// std::u8string. This way, we have a unified way of dealing with the result
	// independently of what the result is.
	auto get_string_view_from_path = [](const auto& fname) -> std::string_view {
		if constexpr (std::is_same_v<std::string, decltype(fname)>) {
			return {fname};
		} else {
			const auto* data = reinterpret_cast<const char*>(fname.data());
			return {data, fname.size()};
		}
	};
	for (const auto& entry : fs::directory_iterator(source, ec)) {
		if (!entry.is_regular_file() && !entry.is_symlink()) {
			continue;
		}
		const fs::path& path = entry.path();
		// Annoyingly, this could be either std::string or std::u8string,
		// depending on the version of the standard, and on parameters of the
		// standard library being used.
		const auto       file     = path.filename().generic_u8string();
		std::string_view filename = get_string_view_from_path(file);
		// But of course, std::regex_match doesn't accept std::string_view...
		if (std::regex_match(filename.cbegin(), filename.cend(), re)) {
			files.emplace_back(
					get_string_view_from_path(path.generic_u8string()));
		}
	}
	if (ec != std::error_code{}) {
		std::cerr << "Error while listing files: " << ec.message() << std::endl;
	}
	return ec.value();
}

#else
// This system has glob.h
static int U7ListFilesImp(
		const std::string& directory, const std::string& mask,
		FileList& files) {
	glob_t      globres;
	std::string path(directory + '/' + mask);
	int         err = glob(path.c_str(), GLOB_NOSORT, nullptr, &globres);

	switch (err) {
	case 0:    // OK
		for (size_t i = 0; i < globres.gl_pathc; i++) {
			files.push_back(globres.gl_pathv[i]);
		}
		globfree(&globres);
		return 0;
	case 3:    // no matches
		return 0;
	default:    // error
		std::cerr << "Glob error " << err << std::endl;
		return err;
	}
}

#endif

int U7ListFiles(
		const std::string& directory, const std::string& mask,
		FileList& files) {
	string path(get_system_path(directory));
	int    result = U7ListFilesImp(path, mask, files);
#ifdef ANDROID
	// TODO: If SDL ever adds directory traversal to rwops use it instead so
	// that we pick up platform-specific paths and behaviors like this.
	if (result != 0) {
		result = U7ListFilesImp(
				SDL_AndroidGetInternalStoragePath() + ("/" + path), mask,
				files);
	}
#endif
	return result;
}
