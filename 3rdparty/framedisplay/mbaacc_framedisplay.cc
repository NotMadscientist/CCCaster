// MBAACC Frame Display:
//
// Core manager.

#include "mbaacc_framedisplay.h"

#include <cstdio>
#include <cstring>

struct MBAACC_Character_Info {
	int		index;
	const char	*long_name;
	const char	*short_name;
	const char	*file_name;
	int		moon;
};

static const MBAACC_Character_Info mbaacc_character_info[] = {
	{ 3, "C-Tohno Akiha",		"C-Akiha",	"AKIHA",	0	},
	{ 3, "F-Tohno Akiha",		"F-Akiha",	"AKIHA",	1	},
	{ 3, "H-Tohno Akiha",		"H-Akiha",	"AKIHA",	2	},
	{22, "C-Aozaki Aoko",		"C-Aoko",	"AOKO",		0	},
	{22, "F-Aozaki Aoko",		"F-Aoko",	"AOKO",		1	},
	{22, "H-Aozaki Aoko",		"H-Aoko",	"AOKO",		2	},
	{ 1, "C-Arcuied Brunestud",		"C-Arc",	"ARC",		0	},
	{ 1, "F-Arcuied Brunestud",		"F-Arc",	"ARC",		1	},
	{ 1, "H-Arcuied Brunestud",		"H-Arc",	"ARC",		2	},
	{ 2, "C-Ciel",		"C-Ciel",	"CIEL",		0	},
	{ 2, "F-Ciel",		"F-Ciel",	"CIEL",		1	},
	{ 2, "H-Ciel",		"H-Ciel",	"CIEL",		2	},
	{51, "C-Archetype:Earth",		"C-Hime",	"P_ARC",	0	},
	{51, "F-Archetype:Earth",		"F-Hime",	"P_ARC",	1	},
	{51, "H-Archetype:Earth",		"H-Hime",	"P_ARC",	2	},
	{ 5, "C-Hisui",		"C-Hisui",	"HISUI",	0	},
	{ 5, "F-Hisui",		"F-Hisui",	"HISUI",	1	},
	{ 5, "H-Hisui",		"H-Hisui",	"HISUI",	2	},
	{28, "C-Kouma",		"C-Kouma",	"KISHIMA",	0	},
	{28, "F-Kouma",		"F-Kouma",	"KISHIMA",	1	},
	{28, "H-Kouma",		"H-Kouma",	"KISHIMA",	2	},
	{ 6, "C-Kohaku",		"C-Kohaku",	"KOHAKU",	0	},
	{ 6, "F-Kohaku",		"F-Kohaku",	"KOHAKU",	1	},
	{ 6, "H-Kohaku",		"H-Kohaku",	"KOHAKU",	2	},
	{18, "C-Len",		"C-Len",	"LEN",		0	},
	{18, "F-Len",		"F-Len",	"LEN",		1	},
	{18, "H-Len",		"H-Len",	"LEN",		2	},
	{ 8, "C-Miyako Arima",		"C-Miyako",	"MIYAKO",	0	},
	{ 8, "F-Miyako Arima",		"F-Miyako",	"MIYAKO",	1	},
	{ 8, "H-Miyako Arima",		"H-Miyako",	"MIYAKO",	2	},
	{14, "C-Mech-Hisui",		"C-M.Hisui",	"M_HISUI",	0	},
	{14, "F-Mech-Hisui",		"F-M.Hisui",	"M_HISUI",	1	},
	{14, "H-Mech-Hisui",		"H-M.Hisui",	"M_HISUI",	2	},
	{25, "C-Neko Arc Chaos",	"C-NAC",	"NECHAOS",	0	},
	{25, "F-Neko Arc Chaos",	"F-NAC",	"NECHAOS",	1	},
	{25, "H-Neko Arc Chaos",	"H-NAC",	"NECHAOS",	2	},
	{15, "C-Nanaya Shiki",		"C-Nanaya",	"NANAYA",	0	},
	{15, "F-Nanaya Shiki",		"F-Nanaya",	"NANAYA",	1	},
	{15, "H-Nanaya Shiki",		"H-Nanaya",	"NANAYA",	2	},
	{20, "C-Neko Arc",		"C-Neko",	"NECO",		0	},
	{20, "F-Neko Arc",		"F-Neko",	"NECO",		1	},
	{20, "H-Neko Arc",		"H-Neko",	"NECO",		2	},
	{10, "C-Nero Chaos",		"C-Nero",	"NERO",		0	},
	{10, "F-Nero Chaos",		"F-Nero",	"NERO",		1	},
	{10, "H-Nero Chaos",		"H-Nero",	"NERO",		2	},
	{19, "C-Powered Ciel",		"C-P.Ciel",	"P_CIEL",		0	},
	{19, "F-Powered Ciel",		"F-P.Ciel",	"P_CIEL",		1	},
	{19, "H-Powered Ciel",		"H-P.Ciel",	"P_CIEL",		2	},
	{30, "C-Riesbyfe Strideberg",		"C-Ries",	"RIES",		0	},
	{30, "F-Riesbyfe Strideberg",		"F-Ries",	"RIES",		1	},
	{30, "H-Riesbyfe Strideberg",		"H-Ries",	"RIES",		2	},
	{31, "C-Michael Roa Valdamjong",		"C-Roa",	"ROA",		0	},
	{31, "F-Michael Roa Valdamjong",		"F-Roa",	"ROA",		1	},
	{31, "H-Michael Roa Valdamjong",		"H-Roa",	"ROA",		2	},
	{33, "C-Ryougi Shiki",		"C-Ryougi",	"RYOUGI",	0	},
	{33, "F-Ryougi Shiki",		"F-Ryougi",	"RYOUGI",	1	},
	{33, "H-Ryougi Shiki",		"H-Ryougi",	"RYOUGI",	2	},
	{17, "C-Yumizuka Satsuki",		"C-Satsuki",	"SATSUKI",	0	},
	{17, "F-Yumizuka Satsuki",		"F-Satsuki",	"SATSUKI",	1	},
	{17, "H-Yumizuka Satsuki",		"H-Satsuki",	"SATSUKI",	2	},
	{ 0, "C-Sion Eltnam Atlasia",		"C-Sion",	"SION",		0	},
	{ 0, "F-Sion Eltnam Atlasia",		"F-Sion",	"SION",		1	},
	{ 0, "H-Sion Eltnam Atlasia",		"H-Sion",	"SION",		2	},
	{29, "C-Seifuku Akiha",		"C-S.Akiha",	"S_AKIHA",	0	},
	{29, "F-Seifuku Akiha",		"F-S.Akiha",	"S_AKIHA",	1	},
	{29, "H-Seifuku Akiha",		"H-S.Akiha",	"S_AKIHA",	2	},
	{ 7, "C-Tohno Shiki",		"C-Tohno",	"SHIKI",	0	},
	{ 7, "F-Tohno Shiki",		"F-Tohno",	"SHIKI",	1	},
	{ 7, "H-Tohno Shiki",		"H-Tohno",	"SHIKI",	2	},
	{13, "C-Akiha Vermillion",		"C-V.Akiha",	"AKAAKIHA",	0	},
	{13, "F-Akiha Vermillion",		"F-V.Akiha",	"AKAAKIHA",	1	},
	{13, "H-Akiha Vermillion",		"H-V.Akiha",	"AKAAKIHA",	2	},
	{11, "C-Sion TATARI",		"C-V.Sion",	"V_SION",	0	},
	{11, "F-Sion TATARI",		"F-V.Sion",	"V_SION",	1	},
	{11, "H-Sion TATARI",		"H-V.Sion",	"V_SION",	2	},
	{ 9, "C-Warachia",		"C-Warachia",	"WARAKIA",	0	},
	{ 9, "F-Warachia",		"F-Warachia",	"WARAKIA",	1	},
	{ 9, "H-Warachia",		"H-Warachia",	"WARAKIA",	2	},
	{12, "C-Red Arcueid",		"C-Warc",	"WARC",		0	},
	{12, "F-Red Arcueid",		"F-Warc",	"WARC",		1	},
	{12, "H-Red Arcueid",		"H-Warc",	"WARC",		2	},
	{23, "C-White Len",		"C-W.Len",	"WLEN",		0	},
	{23, "F-White Len",		"F-W.Len",	"WLEN",		1	},
	{23, "H-White Len",		"H-W.Len",	"WLEN",		2	},

	// Unused for palettes
	// {"C-Mech-Hisui M",		"C-M.Hisui M",	"M_HISUI_M",	0	},
	// {"F-Mech-Hisui M",		"F-M.Hisui M",	"M_HISUI_M",	1	},
	// {"H-Mech-Hisui M",		"H-M.Hisui M",	"M_HISUI_M",	2	},
	// {"C-Kohaku M",		"C-Kohaku M",	"KOHAKU_M",	0	},
	// {"F-Kohaku M",		"F-Kohaku M",	"KOHAKU_M",	1	},
	// {"H-Kohaku M",		"H-Kohaku M",	"KOHAKU_M",	2	},
	// {"C-M.Hisui P",		"C-M.Hisui P",	"M_HISUI_P",	0	},
	// {"F-M.Hisui P",		"F-M.Hisui P",	"M_HISUI_P",	1	},
	// {"H-M.Hisui P",		"H-M.Hisui P",	"M_HISUI_P",	2	},
	// {"C-Neko Arc P",	"C-Neko P",	"NECO_P",	0	},
	// {"F-Neko Arc P",	"F-Neko P",	"NECO_P",	1	},
	// {"H-Neko Arc P",	"H-Neko P",	"NECO_P",	2	},
	// {"Effect",		"EFFECT",	"EFFECT",	-1	},
};

static const int mbaacc_ncharacter_info = sizeof(mbaacc_character_info)/sizeof(mbaacc_character_info[0]);

const char *MBAACC_FrameDisplay::get_character_long_name(int n) {
	if (n < 0 || n >= mbaacc_ncharacter_info) {
		return FrameDisplay::get_character_long_name(n);
	}
	return mbaacc_character_info[n].long_name;
}

const char *MBAACC_FrameDisplay::get_character_name(int n) {
	if (n < 0 || n >= mbaacc_ncharacter_info) {
		return FrameDisplay::get_character_name(n);
	}
	return mbaacc_character_info[n].short_name;
}

int MBAACC_FrameDisplay::get_character_count() const {
	return mbaacc_ncharacter_info;
}

int MBAACC_FrameDisplay::get_character_index(int n) {
	if (n < 0 || n >= mbaacc_ncharacter_info)
		return -1;
	return mbaacc_character_info[n].index;
}

int MBAACC_FrameDisplay::get_sequence_count() {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.get_sequence_count();
}

bool MBAACC_FrameDisplay::has_sequence(int n) {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.has_sequence(n);
}
const char *MBAACC_FrameDisplay::get_sequence_name(int n) {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.get_sequence_name(n);
}

const char *MBAACC_FrameDisplay::get_sequence_move_name(int n, int *dmeter) {
	if (!m_initialized) {
		return 0;
	}

	const char *str = m_character_data.get_sequence_move_name(n, dmeter);

	if (!str) {
		const char *ch_name = get_character_name(m_character);

		if (ch_name && !strstr(ch_name, "EFFECT")) {
			switch(n) {
			case 0:	str = "5"; break;
			case 1: str = "5A"; break;
			case 2: str = "5B"; break;
			case 3: str = "5C"; break;
			case 4: str = "2A"; break;
			case 5: str = "2B"; break;
			case 6: str = "2C"; break;
			case 7: str = "j.A"; break;
			case 8: str = "j.B"; break;
			case 9: str = "j.C"; break;
			case 10: str = "6"; break;
			case 11: str = "4"; break;
			case 12: str = "5->2"; break;
			case 13: str = "2"; break;
			case 14: str = "2->5"; break;
			case 17: str = "4 Guard"; break;
			case 18: str = "3 Guard"; break;
			case 19: str = "j.4 Guard";
			case 35: str = "9"; break;
			case 36: str = "8"; break;
			case 37: str = "7"; break;
			case 38: str = "j.9"; break;
			case 39: str = "j.8"; break;
			case 40: str = "j.7"; break;
			case 50: str = "intro"; break;
			case 52: str = "win pose"; break;
			case 250: str = "heat"; break;
			case 255: str = "circuit spark"; break;
			}
		}
	}

	return str;
}

int MBAACC_FrameDisplay::get_frame_count() {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.get_frame_count(m_sequence);
}

int MBAACC_FrameDisplay::get_subframe() {
	return m_subframe + m_subframe_base;
}

int MBAACC_FrameDisplay::get_subframe_count() {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.get_subframe_count(m_sequence);
}

void MBAACC_FrameDisplay::set_frame(int n) {
	if (!m_initialized) {
		return;
	}

	m_frame = m_character_data.find_frame(m_sequence, n);

	m_subframe = 0;

	m_subframe_base = m_character_data.count_subframes(m_sequence, m_frame);
	m_subframe_next = m_character_data.get_subframe_length(m_sequence, m_frame);
}

void MBAACC_FrameDisplay::set_sequence(int n) {
	if (!m_initialized) {
		return;
	}

	m_sequence = m_character_data.find_sequence(n, n<m_sequence?-1:1);

	m_subframe_base = 0;
	m_subframe = 0;

	set_frame(0);
}

void MBAACC_FrameDisplay::set_palette(int n) {
	if (!m_initialized) {
		return;
	}

	m_palette = n%36;

	m_character_data.set_palette(n);
}

void MBAACC_FrameDisplay::set_active_character(int n) {
	if (!m_initialized) {
		return;
	}

	if (n == m_character) {
		return;
	}

	if (n < 0 || n >= (mbaacc_ncharacter_info)) {
		return;
	}

	bool need_gfx = 1;

	if (m_character >= 0 && m_character < mbaacc_ncharacter_info) {
		if (!strcmp(mbaacc_character_info[m_character].file_name, mbaacc_character_info[n].file_name)) {
			need_gfx = 0;
		}
	}

	if (need_gfx) {
		m_character_data.free();
	} else {
		m_character_data.free_frame_data();
	}

	if (m_character_data.load(&m_pack, mbaacc_character_info[n].file_name, mbaacc_character_info[n].moon)) {
		if (need_gfx) {
			m_character_data.load_graphics(&m_pack);
		}
	}
	m_character = n;

	set_palette(m_palette);
	set_sequence(0);
	set_frame(0);
}

bool MBAACC_FrameDisplay::init(const char *filename) {
	if (m_initialized) {
		return 1;
	}

	if (!m_pack.open_pack(filename)) {
		return 0;
	}

	// finish up
	if (!FrameDisplay::init()) {
		free();

		return 0;
	}

	// set defaults
	m_character = -1;
	set_active_character(0);

	return 1;
}

bool MBAACC_FrameDisplay::init() {
	if (m_initialized) {
		return 1;
	}

	if (!init("0002.p")) {
		return 0;
	}

	return 1;
}

void MBAACC_FrameDisplay::free() {
	m_pack.close_pack();

	m_character_data.free();

	m_character = -1;

	m_subframe = 0;
	m_subframe_base = 0;
	m_subframe_next = 0;

	FrameDisplay::free();
}

void MBAACC_FrameDisplay::render(const RenderProperties *properties) {
	if (!m_initialized) {
		return;
	}

	m_character_data.render(properties, m_sequence, m_frame);
}

Clone *MBAACC_FrameDisplay::make_clone() {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.make_clone(m_sequence, m_frame);
}

void MBAACC_FrameDisplay::flush_texture() {
	if (!m_initialized) {
		return;
	}

	m_character_data.flush_texture();
}

void MBAACC_FrameDisplay::render_frame_properties(bool detailed, int scr_width, int scr_height) {
	if (!m_initialized) {
		return;
	}

	m_character_data.render_frame_properties(detailed, scr_width, scr_height, m_sequence, m_frame);
}

void MBAACC_FrameDisplay::command(FrameDisplayCommand command, const void *param) {
	if (!m_initialized) {
		return;
	}

	switch(command) {
	case COMMAND_CHARACTER_NEXT:
		if (m_character == -1) {
			set_active_character(0);
		} else {
			set_active_character((m_character+1)%(mbaacc_ncharacter_info));
		}
		break;
	case COMMAND_CHARACTER_PREV:
		if (m_character == -1) {
			set_active_character(0);
		} else {
			set_active_character((m_character+(mbaacc_ncharacter_info)-1)%mbaacc_ncharacter_info);
		}
		break;
	case COMMAND_CHARACTER_SET:
		if (!param) {
			break;
		}
		set_active_character((int)(*(unsigned int *)param)%(mbaacc_ncharacter_info));
		break;
	case COMMAND_PALETTE_NEXT:
		set_palette((m_palette+1)%36);
		break;
	case COMMAND_PALETTE_PREV:
		set_palette((m_palette+35)%36);
		break;
	case COMMAND_PALETTE_SET:
		if (!param) {
			break;
		}
		set_palette((int)(*(int *)param)%36);
		break;
	case COMMAND_SEQUENCE_NEXT:
		set_sequence(m_sequence+1);
		break;
	case COMMAND_SEQUENCE_PREV:
		set_sequence(m_sequence-1);
		break;
	case COMMAND_SEQUENCE_SET:
		if (!param) {
			break;
		}
		set_sequence(*(int *)param);
		break;
	case COMMAND_FRAME_NEXT:
		set_frame(m_frame+1);
		break;
	case COMMAND_FRAME_PREV:
		set_frame(m_frame-1);
		break;
	case COMMAND_FRAME_SET:
		if (!param) {
			break;
		}
		set_frame(*(int *)param);
		break;
	case COMMAND_SUBFRAME_NEXT:
		m_subframe++;
		if (m_subframe >= m_subframe_next) {
			set_frame(m_frame+1);
		}
		break;
	case COMMAND_SUBFRAME_PREV:
		--m_subframe;
		if (m_subframe < 0) {
			set_frame(m_frame+1);
			m_subframe = m_subframe_next - 1;
		}
		break;
	case COMMAND_SUBFRAME_SET:
		break;
	}
}

const char *MBAACC_FrameDisplay::get_current_sprite_filename() {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.get_current_sprite_filename(m_sequence, m_frame);
}

bool MBAACC_FrameDisplay::save_current_sprite(const char *filename) {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.save_current_sprite(filename, m_sequence, m_frame);
}

int MBAACC_FrameDisplay::save_all_character_sprites(const char *directory) {
	if (!m_initialized) {
		return 0;
	}

	return m_character_data.save_all_character_sprites(directory);
}

MBAACC_FrameDisplay::MBAACC_FrameDisplay() {
	m_character = -1;
	m_subframe = 0;
	m_subframe_base = 0;
	m_subframe_next = 0;
}

MBAACC_FrameDisplay::~MBAACC_FrameDisplay() {
	// cleanup will do the work
}

