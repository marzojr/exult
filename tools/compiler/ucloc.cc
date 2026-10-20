/**
 ** Ucloc.cc - Source location.
 **
 ** Written: 1/0/01 - JSF
 **/

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

#include "ucloc.h"

#include "ignore_unused_variable_warning.h"

#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string_view>

using std::cerr;
using std::endl;
using std::strcmp;
using std::strcpy;
using std::strlen;

std::vector<char*> Uc_location::source_names;

char*              Uc_location::cur_source         = nullptr;
int                Uc_location::cur_line           = 0;
int                Uc_location::cur_column         = 1;
int                Uc_location::cur_length         = 1;
int                Uc_location::next_line          = 0;
int                Uc_location::next_column        = 1;
const Uc_location* Uc_location::diagnostic_context = nullptr;
int                Uc_location::num_errors         = 0;
bool               Uc_location::strict_mode        = false;
bool               Uc_location::color_output       = false;
bool               Uc_location::goto_warn          = true;
bool               Uc_location::integer_warn       = true;
bool               Uc_location::shapefun_warn      = true;

/*
 *  Set current source and line #.
 */

void Uc_location::set_cur(const char* s, int l, int c) {
	cur_line = next_line = l;
	cur_column = next_column = c;
	cur_length               = 1;
	cur_source               = nullptr;    // See if already here.
	for (auto* name : source_names) {
		if (strcmp(s, name) == 0) {
			cur_source = name;
			break;
		}
	}
	if (!cur_source) {    // 1st time.
		const int len = strlen(s);
		cur_source    = new char[len + 1];
		strcpy(cur_source, s);
		source_names.push_back(cur_source);
	}
}

namespace {
	std::map<std::string, std::vector<std::string>> source_lines;
}

void Uc_location::register_source(const char* name, const char* path) {
	if (source_lines.find(name) != source_lines.end()) {
		return;
	}
	std::ifstream input(path);
	if (!input) {
		return;
	}
	auto&       lines = source_lines[name];
	std::string text;
	while (std::getline(input, text)) {
		if (!text.empty() && text.back() == '\r') {
			text.pop_back();
		}
		lines.push_back(text);
	}
}

void Uc_location::advance(const char* text, int size) {
	cur_line        = next_line;
	cur_column      = next_column;
	cur_length      = 0;
	bool first_line = true;
	for (int i = 0; i < size; ++i) {
		if (text[i] == '\n') {
			++next_line;
			next_column = 1;
			first_line  = false;
		} else {
			++next_column;
			if (first_line) {
				++cur_length;
			}
		}
	}
}

Uc_location Uc_location::next_location() {
	Uc_location loc;
	loc.line   = next_line;
	loc.column = next_column;
	loc.length = 1;
	return loc;
}

void Uc_location::assemble_message(
		const char* msg, const char* source_file, int line_num, int column, int length, Severity severity) {
	if (!source_file) {
		source_file = "<unknown>";
	}
	const bool is_error = severity == Severity::error;
	// For colorization.
	constexpr static const std::string_view error_prefix   = "\033[1;31merror:\033[0m ";
	constexpr static const std::string_view warning_prefix = "\033[1;35mwarning:\033[0m ";
	constexpr static const std::string_view source_prefix  = "\033[1m";
	constexpr static const std::string_view line_prefix    = "\033[1;34m";
	constexpr static const std::string_view reset_style    = "\033[0m";
	if (color_output) {
		cerr << source_prefix << source_file << ":" << reset_style << line_prefix << line_num << ":" << column << ": "
			 << reset_style;
		if (severity == Severity::note) {
			cerr << "\033[1;36mnote:\033[0m ";
		} else if (is_error) {
			cerr << error_prefix;
		} else {
			cerr << warning_prefix;
		}
		cerr << msg << reset_style << endl;
	} else {
		cerr << source_file << ':' << line_num << ':' << column << ": ";
		if (severity == Severity::note) {
			cerr << "note: ";
		} else if (is_error) {
			cerr << "error: ";
		} else {
			cerr << "warning: ";
		}
		cerr << msg << endl;
	}
	const auto file = source_lines.find(source_file ? source_file : "");
	if (file == source_lines.end() || line_num < 1 || static_cast<size_t>(line_num) > file->second.size()) {
		return;
	}
	const auto& text = file->second[line_num - 1];
	// Expand tabs consistently in both the source line and the underline.
	std::string expanded;
	std::string marker;
	for (size_t i = 0; i < text.size(); ++i) {
		const size_t width = text[i] == '\t' ? 8 - expanded.size() % 8 : 1;
		expanded.append(width, text[i] == '\t' ? ' ' : text[i]);
		if (i < static_cast<size_t>(column - 1)) {
			marker.append(width, ' ');
		} else if (i < static_cast<size_t>(column - 1 + length)) {
			if (i == static_cast<size_t>(column - 1)) {
				marker += '^';
				marker.append(width - 1, '~');
			} else {
				marker.append(width, '~');
			}
		}
	}
	if (static_cast<size_t>(column - 1) >= text.size()) {
		marker.append(static_cast<size_t>(column - 1) - text.size(), ' ');
		marker += '^';
	}
	cerr << "  " << expanded << '\n';
	if (color_output) {
		cerr << "\033[1;32m";
	}
	cerr << "  " << marker;
	if (color_output) {
		cerr << reset_style;
	}
	cerr << endl;
}

/*
 *  Print error for current parser location.
 */

void Uc_location::yyerror(preformatted_t tag, const char* s) {
	const Uc_location loc;
	ignore_unused_variable_warning(tag);
	assemble_message(s, loc.source, loc.line + 1, loc.column, loc.length, Severity::error);
	num_errors++;
}

/*
 *  Print warning for current parser location.
 */

void Uc_location::yywarning(preformatted_t tag, const char* s) {
	const Uc_location loc;
	ignore_unused_variable_warning(tag);
	assemble_message(s, loc.source, loc.line + 1, loc.column, loc.length, Severity::warning);
}

namespace {
	std::string format_message(const char* format, va_list args) {
		va_list copy;
		va_copy(copy, args);
		const int size = vsnprintf(nullptr, 0, format, copy);
		va_end(copy);
		if (size < 0) {
			return "unable to format diagnostic";
		}
		std::string message(static_cast<size_t>(size) + 1, '\0');
		vsnprintf(message.data(), message.size(), format, args);
		message.resize(size);
		return message;
	}
}    // namespace

void Uc_location::note(const char* format, ...) const {
	va_list args;
	va_start(args, format);
	const auto message = format_message(format, args);
	va_end(args);
	assemble_message(message.c_str(), source, line + 1, column, length, Severity::note);
}

/*
 *  Print error for stored position.
 */

void Uc_location::error(const char* format, ...) const {
	va_list args;
	va_start(args, format);
	const auto message = format_message(format, args);
	assemble_message(message.c_str(), source, line + 1, column, length, Severity::error);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for stored position.
 */

void Uc_location::warning(const char* format, ...) const {
	va_list args;
	va_start(args, format);
	const auto message = format_message(format, args);
	assemble_message(message.c_str(), source, line + 1, column, length, Severity::warning);
	va_end(args);
}

/*
 *  Print error for current parser location.
 */

void Uc_location::yyerror(const char* format, ...) {
	const Uc_location loc;
	va_list           args;
	va_start(args, format);
	const auto message = format_message(format, args);
	assemble_message(message.c_str(), loc.source, loc.line + 1, loc.column, loc.length, Severity::error);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for current parser location.
 */

void Uc_location::yywarning(const char* format, ...) {
	const Uc_location loc;
	va_list           args;
	va_start(args, format);
	const auto message = format_message(format, args);
	assemble_message(message.c_str(), loc.source, loc.line + 1, loc.column, loc.length, Severity::warning);
	va_end(args);
}

/*
 *  Print error for stored position.
 */

void Uc_location::error(std::string& buffer, const char* format, ...) const {
	va_list args;
	va_start(args, format);
	buffer = format_message(format, args);
	assemble_message(buffer.c_str(), source, line + 1, column, length, Severity::error);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for stored position.
 */

void Uc_location::warning(std::string& buffer, const char* format, ...) const {
	va_list args;
	va_start(args, format);
	buffer = format_message(format, args);
	assemble_message(buffer.c_str(), source, line + 1, column, length, Severity::warning);
	va_end(args);
}

/*
 *  Print error for current parser location.
 */

void Uc_location::yyerror(std::string& buffer, const char* format, ...) {
	const Uc_location loc;
	va_list           args;
	va_start(args, format);
	buffer = format_message(format, args);
	assemble_message(buffer.c_str(), loc.source, loc.line + 1, loc.column, loc.length, Severity::error);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for current parser location.
 */

void Uc_location::yywarning(std::string& buffer, const char* format, ...) {
	const Uc_location loc;
	va_list           args;
	va_start(args, format);
	buffer = format_message(format, args);
	assemble_message(buffer.c_str(), loc.source, loc.line + 1, loc.column, loc.length, Severity::warning);
	va_end(args);
}
