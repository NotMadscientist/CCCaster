// MBAACC character manager:
//
// Handles all individual character data.

#include "mbaacc_framedisplay.h"
#include "render.h"
#include "clone.h"

#include <cstdlib>
#include <cstring>

static void copy_hitbox_to_rect(rect_t *rect, MBAACC_Hitbox *hitbox) {
	rect->x1 = (hitbox->x1)*2;
	rect->y1 = (hitbox->y1)*2;
	rect->x2 = (hitbox->x2)*2;
	rect->y2 = (hitbox->y2)*2;
}

MBAACC_Frame *MBAACC_Character::get_frame(int seq_id, int frame_id) {
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}

	if ((unsigned int)frame_id < seq->nframes) {
		return &seq->frames[frame_id];
	}
	
	return 0;
}

void MBAACC_Character::set_render_properties(const MBAACC_Frame *frame, Texture *texture) {
	texture->blend_mode(frame->AF.blend_mode);
	if (frame->AF.blend_mode != 0) {
		texture->color((float)frame->AF.red/255.0f,
				 (float)frame->AF.green/255.0f,
				 (float)frame->AF.blue/255.0f);
		texture->alpha((float)frame->AF.alpha/255.0f);
	}
	
	texture->rotate_clear();
	if (frame->AF.z_rotation != 0.0f) {
		texture->rotate_z(frame->AF.z_rotation);
	}
	if (frame->AF.x_rotation != 0.0f) {
		texture->rotate_x(frame->AF.x_rotation);
	}
	if (frame->AF.y_rotation != 0.0f) {
		texture->rotate_y(frame->AF.y_rotation);
	}
	
	if (frame->AF.has_zoom) {
		texture->scale(frame->AF.zoom_x, frame->AF.zoom_y);
	} else {
		texture->scale_clear();
	}
}

void MBAACC_Character::render(const RenderProperties *properties, int seq_id, int frame_id) {
	if (!m_loaded) {
		return;
	}
	
	MBAACC_Frame *frame = get_frame(seq_id, frame_id);
	
	if (!frame) {
		return;
	}
	
	if (properties->display_sprite && frame->AF.active && frame->AF.frame >= 0) {
		// render sprite
		if (!m_texture || m_last_sprite_id != frame->AF.frame) {
			if (m_texture) {
				delete m_texture;
			}
			
			m_texture = m_cg.draw_texture(frame->AF.frame, m_palettes[m_active_palette], 1, 0);
			
			m_last_sprite_id = frame->AF.frame;
		}
		
		if (m_texture) {
			set_render_properties(frame, m_texture);
			
			m_texture->draw((frame->AF.offset_x-128)*2, (frame->AF.offset_y-224)*2, 2);
		}
	}
	
	// render collision box
	if (properties->display_collision_box) {
		if (frame->hitboxes[0]) {
			rect_t rect;
			
			copy_hitbox_to_rect(&rect, frame->hitboxes[0]);
			
			render_boxes(BOX_COLLISION, &rect, 1, properties->display_solid_boxes);
		}
	}
	
	// render hitboxes
	rect_t rects[32];
	int nrects;
	
	if (properties->display_hit_box) {
		nrects = 0;
		for (int i = 1; i < 11; ++i) {
			if (frame->hitboxes[i]) {
				copy_hitbox_to_rect(&rects[nrects], frame->hitboxes[i]);
				
				++nrects;
			}
		}
	
		if (nrects > 0) {
			render_boxes(BOX_HIT, rects, nrects, properties->display_solid_boxes);
		}
	}
	
	// render damage boxes
	if (properties->display_attack_box) {
		nrects = 0;
		for (int i = 25; i < 33; ++i) {
			if (frame->hitboxes[i]) {
				copy_hitbox_to_rect(&rects[nrects], frame->hitboxes[i]);
				
				++nrects;
			}
		}
	
		if (nrects > 0) {
			render_boxes(BOX_ATTACK, rects, nrects, properties->display_solid_boxes);
		}
	}

	// render clash boxes
	if (properties->display_clash_box) {
		nrects = 0;
		for (int i = 11; i < 25; ++i) {
			if (frame->hitboxes[i]) {
				copy_hitbox_to_rect(&rects[nrects], frame->hitboxes[i]);
				
				++nrects;
			}
		}
	
		if (nrects > 0) {
			render_boxes(BOX_CLASH, rects, nrects, properties->display_solid_boxes);
		}
	}
}

Clone *MBAACC_Character::make_clone(int seq_id, int fr_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Frame *frame = get_frame(seq_id, fr_id);
	
	if (!frame) {
		return 0;
	}
	
	Clone *clone = new Clone;
	
	if (frame->AF.active && frame->AF.frame >= 0) {
		Texture *texture;
		
		texture = m_cg.draw_texture(frame->AF.frame, m_palettes[m_active_palette], 1, 0);
		
		if (texture) {
			set_render_properties(frame, texture);
			
			clone->init_texture(texture, (frame->AF.offset_x-128)*2, (frame->AF.offset_y-224)*2, 2);
		}
	}
	
	CloneHitbox hitboxes[33]; // should be enough!
	CloneHitbox *box = hitboxes;
	
	for (int i = 0; i < 33; ++i) {
		if (!frame->hitboxes[i]) {
			continue;
		}
		
		copy_hitbox_to_rect(&box->rect, frame->hitboxes[i]);
		
		if (i >= 25) {
			box->type = BOX_ATTACK;
		} else if (i >= 11) {
			box->type = BOX_CLASH;
		} else if (i == 0) {
			box->type = BOX_COLLISION;
		} else {
			box->type = BOX_HIT;
		}
		
		++box;
	}
	
	int nhitboxes = box - hitboxes;
	
	clone->init_hitboxes(hitboxes, nhitboxes);
	
	return clone;
}

void MBAACC_Character::flush_texture(void) {
	if (!m_loaded) {
		return;
	}
	
	if (m_texture) {
		delete m_texture;
		
		m_texture = 0;
	}
}

void MBAACC_Character::render_frame_properties(bool detailed, int scr_width, int scr_height, int seq_id, int fr_id) {
	if (!m_loaded) {
		return;
	}
	
	MBAACC_Frame *frame = get_frame(seq_id, fr_id);
	
	if (!frame || !frame->AS) {
		return;
	}
	
	int x = scr_width - 185;
	int y = 8;
	char str[512];
	
	y += render_shaded_line(x, y, 180, 'y');
	
	if (frame->AF.active) {
		y += render_shaded_text(x, y, "$yDuration:$. %d",
				frame->AF.duration);
	}
	
	y += render_shaded_text(x, y, "$yState   :$. %s",
		frame->AS->stand_state == 0 ? "Standing"
		: frame->AS->stand_state == 1 ? "Airborne"
		: frame->AS->stand_state == 2 ? "Crouching" : "-");
	
	y += render_line(x, y, 180, 'y');
	
	// this is wrong.
	/*
	y += render_shaded_text(x, y, "$yNext action:");
	if (frame->AF.jump_to_seq >= 0) {
		y += render_shaded_text(x, y, "$y-$. Go to seq %3.3d", frame->AF.jump_to_seq);
	} else if (frame->AS->move_to_frame >= 0) {
		y += render_shaded_text(x, y, "$y-$. Go to state %d", frame->AS->move_to_frame + 1);
	} else {
		y += render_shaded_text(x, y, "$y-$.");
	}
	 */
	
	y += render_line(x, y, 180, 'y');
	
	y += render_shaded_text(x, y, "$t        HORIZ   VERT");
	
	strcpy(str, "$ySpeed:$. ");
	if (frame->AS->speed_flags & 0x10) {
		sprintf(str+strlen(str), "%6d ", frame->AS->speed_horz);
	} else {
		strcat(str, "------ ");
	}
		
	if (frame->AS->speed_flags & 0x1) {
		sprintf(str+strlen(str), "%6d", frame->AS->speed_vert);
	} else {
		strcat(str, "------");
	}
	
	strcat(str, "\n$yAccel:$. ");
	if (frame->AS->speed_flags & 0x20) {
		sprintf(str+strlen(str), "%6d ", frame->AS->accel_horz);
	} else {
		strcat(str, "------ ");
	}
		
	if (frame->AS->speed_flags & 0x2) {
		sprintf(str+strlen(str), "%6d", frame->AS->accel_vert);
	} else {
		strcat(str, "------");
	}

	y += render_shaded_text(x, y, str);
	
	y += render_line(x, y, 180, 'y');
	
	if (frame->AT) {
		y += render_shaded_text(x, y,
			"$y         CRO STA AIR\n" \
			"$yBlock  :$. %s %s %s\n\n" \
			"$yDamage   :$. %-4d $r%-4d\n" \
			"$yProration:$. %d%%\n" \
			"$yCircuit +:$. %.1f%%",
			frame->AT->guard_flags & 0x400 ? " - "
				: frame->AT->guard_flags & 4 ? " Y " : " N ",
			frame->AT->guard_flags & 0x100 ? " - "
				: frame->AT->guard_flags & 1 ? " Y " : " N ",
			frame->AT->guard_flags & 0x200 ? " - "
				: frame->AT->guard_flags & 2 ? " Y " : " N ",
			frame->AT->damage,
			frame->AT->red_damage,
			frame->AT->proration != 0 ? frame->AT->proration : 100,
			frame->AT->circuit_gain/100.0f);
		
		y += render_line(x, y, 180, 'y');
	}
	
}

void MBAACC_Character::load_graphics(MBAACC_Pack *pack) {
	if (!m_loaded) {
		return;
	}
	
	char filename[256];
	
	// read image and alignment data
	sprintf(filename, "%s.CG", m_name);
	if (!m_cg.load(pack, filename)) {
		return;
	}
	
	// done!
}

void MBAACC_Character::unload_graphics() {
	if (!m_loaded) {
		return;
	}
	
	m_cg.free();
}

bool MBAACC_Character::load(MBAACC_Pack *pack, const char *name, int sub_type) {
	if (m_loaded) {
		return 0;
	}
	
	char filename[256];
	
	// read frame data
	sprintf(filename, "%s.HA6", name);
	if (!m_framedata.load(pack, filename)) {
		printf("load fail\n");
		return 0;
	}
	
	sprintf(filename, "%s_r.HA6", name);
	m_framedata.load(pack, filename);
	
	if (sub_type >= 0) {
		sprintf(filename, "%s_%d.HA6", name, sub_type);
		m_framedata.load(pack, filename);
		
		sprintf(filename, "%s_%d_r.HA6", name, sub_type);
		m_framedata.load(pack, filename);
	}

	if (sub_type >= 0) {
		sprintf(filename, "%s_%d_C.TXT", name, sub_type);
		m_framedata.load_move_list(pack, filename);
	}
	
	// read palettes
	m_palettes = new unsigned int *[36];
	for (int i = 0; i < 36; ++i) {
		m_palettes[i] = 0;
	}
	
	sprintf(filename, "%s.pal", name);
	char *data;
	unsigned int size;
	
	if (pack->read_file(filename, &data, &size)) {
		for (int i = 0; i < 36; ++i) {
			m_palettes[i] = new unsigned int[256];
			
			memcpy(m_palettes[i], data + (i * 1024) + 4, 1024);
			
			unsigned int *p = m_palettes[i];
			for (int j = 0; j < 256; ++j) {
				unsigned int v = *p;
				unsigned int alpha = v>>24;
				
				alpha = (alpha != 0) ? 255 : 0;
				
				*p = (v&0xffffff) | (alpha<<24);
				++p;
			}
			
			m_palettes[i][0] &= 0xffffff;
		}
	}
	
	// finish
	m_active_palette = 0;
	
	m_loaded = 1;
	
	m_name = strdup(name);
	
	return 1;
}

void MBAACC_Character::set_palette(int n) {
	if (n < 0 || n > 35) {
		return;
	}
	
	if (!m_palettes[n]) {
		n = 0;
	}
	
	if (n != m_active_palette) {
		flush_texture();
		
		m_active_palette = n;
	}
}

int MBAACC_Character::get_sequence_count() {
	if (!m_loaded) {
		return 0;
	}
	
	return m_framedata.get_sequence_count();
}

bool MBAACC_Character::has_sequence(int seq_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	return seq?1:0;
}

const char *MBAACC_Character::get_sequence_name(int seq_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}
	
	return seq->name.c_str();
}

const char *MBAACC_Character::get_sequence_move_name(int seq_id, int *dmeter) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}
	
	if (!seq->is_move) {
		return 0;
	}
	
	if (dmeter) {
		*dmeter = seq->move_meter;
	}
	
	return seq->move_name.c_str();
}

int MBAACC_Character::get_frame_count(int seq_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}
	
	return seq->nframes;
}

int MBAACC_Character::get_subframe_count(int seq_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}
	
	return seq->subframe_count;
}

int MBAACC_Character::get_subframe_length(int seq_id, int fr_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}
	
	if ((unsigned int)fr_id < seq->nframes && seq->frames[fr_id].AF.active) {
		return seq->frames[fr_id].AF.duration;
	}
	
	return 0;
}

int MBAACC_Character::count_subframes(int seq_id, int fr_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}
	
	unsigned int fr = (unsigned int)fr_id;
	
	if (fr > seq->nframes) {
		fr = seq->nframes;
	}
	
	int count = 0;
	for (unsigned int i = 0; i < fr; ++i) {
		MBAACC_Frame *frame = &seq->frames[i];
		
		if (frame->AF.active) {
			count += frame->AF.duration;
		}
	}
	
	return count;
}

int MBAACC_Character::find_sequence(int n, int direction) {
	if (!m_loaded) {
		return 0;
	}
	
	int nsequences = m_framedata.get_sequence_count();
	
	if (n < 0 || n >= nsequences) {
		if (direction < 0) {
			for (int i = nsequences-1; i >= 0; --i) {
				if (m_framedata.get_sequence(i)) {
					return i;
				}
			}
		} else {
			for (int i = 0; i < nsequences; ++i) {
				if (m_framedata.get_sequence(i)) {
					return i;
				}
			}
		}
	} else {
		int n2 = n + nsequences;
		
		if (direction < 0) {
			for (int i = n2; i > n; --i) {
				int i2 = i%nsequences;
				if (m_framedata.get_sequence(i2)) {
					return i2;
				}
			}
		} else {
			for (int i = n; i < n2; ++i) {
				int i2 = i%nsequences;
				if (m_framedata.get_sequence(i2)) {
					return i2;
				}
			}
		}
	}
	
	return 0;
}

int MBAACC_Character::find_frame(int seq_id, int n) {
	if (!m_loaded) {
		return 0;
	}

	MBAACC_Sequence *seq = m_framedata.get_sequence(seq_id);
	
	if (!seq) {
		return 0;
	}
	
	if (n < 0) {
		return seq->nframes - 1;
	} else if ((unsigned int)n < seq->nframes) {
		return n;
	}
	
	return 0;
}

const char *MBAACC_Character::get_current_sprite_filename(int seq_id, int fr_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Frame *frame = get_frame(seq_id, fr_id);
	
	if (!frame) {
		return 0;
	}

	if (frame->AF.active && frame->AF.frame >= 0) {
		return m_cg.get_filename(frame->AF.frame);
	}
	
	return 0;
}

bool MBAACC_Character::do_sprite_save(int id, const char *filename) {
	Texture *texture;
	
	texture = m_cg.draw_texture(id, m_palettes[m_active_palette], 1, 1);
	
	bool retval = 0;
	
	if (texture) {
		retval = texture->save_to_png(filename, m_palettes[m_active_palette]);
		
		delete texture;
	}
	
	return retval;
}

bool MBAACC_Character::save_current_sprite(const char *filename, int seq_id, int fr_id) {
	if (!m_loaded) {
		return 0;
	}
	
	MBAACC_Frame *frame = get_frame(seq_id, fr_id);
	
	if (!frame) {
		return 0;
	}

	if (frame->AF.active && frame->AF.frame >= 0) {
		return do_sprite_save(frame->AF.frame, filename);
	}
	
	return 0;
}

int MBAACC_Character::save_all_character_sprites(const char *directory) {
	if (!m_loaded) {
		return 0;
	}
	
	int n = m_cg.get_image_count();
	int count = 0;
	
	for (int i = 0; i < n; ++i) {
		const char *image_name = m_cg.get_filename(i);
		
		if (image_name) {
			char filename[2048];
			int len;
			
			sprintf(filename, "%s%s", directory, image_name);
			
			len = strlen(filename);
			if (len > 4 && filename[len-4] == '.') {
				strcpy(filename + len - 4, ".png");
			} else {
				strcat(filename, ".png");
			}
			
			count += do_sprite_save(i, filename) ? 1 : 0;
		}
	}
	
	return count;
}

void MBAACC_Character::free_frame_data() {
	m_framedata.free();
	
	if (m_palettes) {
		for (int i = 0; i < 36; ++i) {
			if (m_palettes[i]) {
				delete[] (unsigned char *)m_palettes[i];
			}
		}
		
		delete[] m_palettes;
		
		m_palettes = 0;
	}
	m_active_palette = 0;
	
	if (m_texture) {
		delete m_texture;
		m_texture = 0;
	}
	m_last_sprite_id = -1;
	
	if (m_name) {
		::free(m_name);
		m_name = 0;
	}
	
	m_loaded = 0;
}

void MBAACC_Character::free_graphics() {
	m_cg.free();
}

void MBAACC_Character::free() {
	free_frame_data();
	
	free_graphics();
}

MBAACC_Character::MBAACC_Character() {
	m_palettes = 0;
	m_active_palette = 0;
	
	m_texture = 0;
	m_last_sprite_id = -1;
	
	m_name = 0;
	
	m_loaded = 0;
}

MBAACC_Character::~MBAACC_Character() {
	free();
}

