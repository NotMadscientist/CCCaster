#include "framedisplay.h"

#include <cstdio>

const char *FrameDisplay::get_character_name(int n) {
	return 0;
}

const char *FrameDisplay::get_character_long_name(int n) {
	return get_character_name(n);
}

int FrameDisplay::get_character() {
	return m_character;
}

int FrameDisplay::get_sequence() {
	return m_sequence;
}

bool FrameDisplay::has_sequence(int n) {
	return 0;
}
const char *FrameDisplay::get_sequence_name(int n) {
	return 0;
}

const char *FrameDisplay::get_sequence_move_name(int n, int *dmeter) {
	return 0;
}

int FrameDisplay::get_sequence_count() {
	return 0;
}

int FrameDisplay::get_frame() {
	return m_frame;
}

int FrameDisplay::get_frame_count() {
	return 0;
}

int FrameDisplay::get_palette() {
	return m_palette;
}

int FrameDisplay::get_subframe() {
	return 0;
}

int FrameDisplay::get_subframe_count() {
	return 0;
}

const char *FrameDisplay::get_current_sprite_filename() {
	return 0;
}

bool FrameDisplay::save_current_sprite(const char *filename) {
	return 0;
}

int FrameDisplay::save_all_character_sprites(const char *directory) {
	return 0;
}

void FrameDisplay::render(const RenderProperties *properties) {
	if (!m_initialized) {
		return;
	}

	// stub
}

Clone *FrameDisplay::make_clone() {
	return 0;
}

void FrameDisplay::flush_texture() {
	if (!m_initialized) {
		return;
	}

	// stub
}

void FrameDisplay::render_frame_properties(bool detailed, int scr_width, int scr_height) {
}

void FrameDisplay::command(FrameDisplayCommand command, const void *param) {
	if (!m_initialized) {
		return;
	}

	// stub
}

bool FrameDisplay::init() {
	m_initialized = 1;

	return 1;
}

bool FrameDisplay::init(const char *filename) {
	return init();
}

void FrameDisplay::free() {
	m_character = 0;
	m_sequence = 0;
	m_state = 0;
	m_frame = 0;
	m_palette = 0;
	m_initialized = 0;
}

FrameDisplay::FrameDisplay() {
	m_character = 0;
	m_sequence = 0;
	m_state = 0;
	m_frame = 0;
	m_palette = 0;
	m_initialized = 0;
}

FrameDisplay::~FrameDisplay() {
	if (m_initialized) {
		free();
	}
}
