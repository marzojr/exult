/*
Copyright (C) 2003-2005  The Pentagram Team
Copyright (C) 2009-2022  The Exult Team

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

#ifndef XMIDIFILE_H_INCLUDED
#define XMIDIFILE_H_INCLUDED

#include "common_types.h"
#include "gamma.h"
#include "ignore_unused_variable_warning.h"
#include "istring.h"

#include <string_view>

class IDataSource;
class OIDataSource;
struct XMidiEvent;
class XMidiEventList;

// Conversion types for Midi files
enum class MidiConversionType {
	GM_TO_MT32        = 0,
	MT32_TO_GM        = 1,
	MT32_TO_GS        = 2,
	MT32_TO_GS127     = 3,
	NO_CONVERSION     = 4,
	GS127_TO_GS       = 5,
	U7VOICE_MT_FILE   = 6,
	XMIDI_MT_FILE     = 7,
	SYX_FILE          = 8,
	SYSEX_IN_MID_FILE = 9
};

// Conversion types for Midi files
enum class SFXConversionType {
	NO_CONVERSION = static_cast<int>(MidiConversionType::NO_CONVERSION),
	GS127_TO_GS = static_cast<int>(MidiConversionType::GS127_TO_GS),
};

constexpr std::string_view to_string(MidiConversionType type) {
	using std::string_view_literals::operator""sv;
	switch (type) {
	case MidiConversionType::GM_TO_MT32:
		return "fakemt32"sv;
	case MidiConversionType::MT32_TO_GS:
		return "gs"sv;
	case MidiConversionType::MT32_TO_GS127:
		return "gs127"sv;
	case MidiConversionType::NO_CONVERSION:
		return "mt32"sv;
	// All of these go to GM conversion.
	case MidiConversionType::MT32_TO_GM:
	case MidiConversionType::GS127_TO_GS:
	case MidiConversionType::U7VOICE_MT_FILE:
	case MidiConversionType::XMIDI_MT_FILE:
	case MidiConversionType::SYX_FILE:
	case MidiConversionType::SYSEX_IN_MID_FILE:
		return "gm"sv;
	}
	return "gm"sv;
}

constexpr MidiConversionType from_string(
		std::string_view s, MidiConversionType default_value) {
	ignore_unused_variable_warning(default_value);
	if (Pentagram::iequals(s, "gs") || Pentagram::iequals(s, "gs127drum")) {
		return MidiConversionType::MT32_TO_GS;
	}
	if (Pentagram::iequals(s, "mt32") || Pentagram::iequals(s, "none")) {
		return MidiConversionType::NO_CONVERSION;
	}
	if (Pentagram::iequals(s, "gs127")) {
		return MidiConversionType::MT32_TO_GS127;
	}
	if (Pentagram::iequals(s, "fakemt32")) {
		return MidiConversionType::GM_TO_MT32;
	}
	return MidiConversionType::MT32_TO_GM;
}

constexpr std::string_view to_string(SFXConversionType type) {
	using std::string_view_literals::operator""sv;
	switch (type) {
	case SFXConversionType::NO_CONVERSION:
		return "mt32"sv;
	// All of these go to GM conversion.
	case SFXConversionType::GS127_TO_GS:
		return "gs"sv;
	}
	return "gs"sv;
}

constexpr SFXConversionType from_string(
		std::string_view s, SFXConversionType default_value) {
	ignore_unused_variable_warning(default_value);
	if (Pentagram::iequals(s, "gs")) {
		return SFXConversionType::GS127_TO_GS;
	}
	if (Pentagram::iequals(s, "mt32") || Pentagram::iequals(s, "none")) {
		return SFXConversionType::NO_CONVERSION;
	}
	if (Pentagram::iequals(s, "gs127")) {
		return SFXConversionType::NO_CONVERSION;
	}
	return SFXConversionType::GS127_TO_GS;
}

class XMidiFile {
protected:
	uint16 num_tracks;

private:
	XMidiEventList** events;

	XMidiEvent* list;
	XMidiEvent* branches;
	XMidiEvent* current;
	XMidiEvent* x_patch_bank_cur;
	XMidiEvent* x_patch_bank_first;

	static const char  mt32asgm[128];
	static const char  mt32asgs[256];
	static const char  gmasmt32[128];
	bool               bank127[16];
	MidiConversionType convert_type;

	bool do_reverb;
	bool do_chorus;
	int  chorus_value;
	int  reverb_value;

	// Midi Volume Curve Modification
	static GammaTable<unsigned char> VolumeCurve;

public:
	XMidiFile() = delete;    // No default constructor
	XMidiFile(IDataSource* source, MidiConversionType pconvert);
	~XMidiFile();

	int number_of_tracks() {
		return num_tracks;
	}

	// External Event list functions
	XMidiEventList* GetEventList(uint32 track);

	XMidiEventList* StealEventList() {
		XMidiEventList* tmp = GetEventList(0);
		events              = nullptr;
		return tmp;
	}

	// Not yet implimented
	// int apply_patch (int track, DataSource *source);

private:
	struct first_state {          // Status,	Data[0]
		XMidiEvent* patch[16];    // 0xC
		XMidiEvent* bank[16];     // 0xB,		0
		XMidiEvent* pan[16];      // 0xB,		7
		XMidiEvent* vol[16];      // 0xB,		10
	};

	// List manipulation
	void CreateNewEvent(int time);

	// Variable length quantity
	int GetVLQ(IDataSource* source, uint32& quant);
	int GetVLQ2(IDataSource* source, uint32& quant);

	void AdjustTimings(uint32 ppqn);    // This is used by Midi's ONLY!
	void ApplyFirstState(first_state& fs, int chan_mask);

	int ConvertNote(
			const int time, const unsigned char status, IDataSource* source,
			const int size);
	int ConvertEvent(
			const int time, const unsigned char status, IDataSource* source,
			const int size, first_state& fs);
	int ConvertSystemMessage(
			const int time, const unsigned char status, IDataSource* source);
	int CreateMT32SystemMessage(
			const int time, uint32 address_base, uint16 address_offset,
			uint32 len, const void* data = nullptr,
			IDataSource* source = nullptr);

	int ConvertFiletoList(
			IDataSource* source, const bool is_xmi, first_state& fs);

	int ExtractTracksFromXmi(IDataSource* source);
	int ExtractTracksFromMid(
			IDataSource* source, const uint32 ppqn, const int num_tracks,
			const bool type1);

	int ExtractTracks(IDataSource* source);

	int  ExtractTracksFromU7V(IDataSource* source);
	int  ExtractTracksFromXMIDIMT(IDataSource* source);
	void InsertDisplayEvents();

	void CreateEventList();
	void DestroyEventList();
};

#endif    // XMIDIFILE_H_INCLUDED
