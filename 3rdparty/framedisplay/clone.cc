// Clone manager.
//
// Renders clones.

#include <cstring>
#include <cstdio>

#include "render.h"
#include "clone.h"

void Clone::init_texture(Texture *texture, int x, int y, int scale) {
	if (texture) {
		if (m_texture) {
			delete[] m_texture;
		}
		m_texture = texture;
	}
	
	m_texture_x = x;
	m_texture_y = y;
	m_texture_scale = scale;
}

void Clone::init_hitboxes(CloneHitbox *hitboxes, int nhitboxes) {
	if (m_rects) {
		delete[] m_rects;
		m_rects = 0;
	}
	
	if (nhitboxes > 0) {
		m_rects = new rect_t[nhitboxes];
		m_nrects = nhitboxes;
		
		int base = 0;
		BoxType box_type = BOX_DEFAULT;
		
		for (int i = 0; i < 4; ++i) {
			int n = 0;
			
			switch(i) {
			case 0: box_type = BOX_COLLISION; break;
			case 1: box_type = BOX_HIT; break;
			case 2: box_type = BOX_ATTACK; break;
			case 3: box_type = BOX_CLASH; break;
			}
			
			for (int j = 0; j < nhitboxes; ++j) {
				if (hitboxes[j].type == box_type) {
					memcpy(&m_rects[base+n], &hitboxes[j].rect, sizeof(rect_t));
					
					++n;
				}
			}
			
			switch(i) {
			case 0: m_nrect_collision = n; break;
			case 1: m_nrect_hit = n; break;
			case 2: m_nrect_attack = n; break;
			case 3: m_nrect_clash = n; break;
			}
			
			base += n;
		}
	}
}

void Clone::render(const RenderProperties *properties) {
	if (properties->display_sprite && m_texture) {
		m_texture->draw(m_texture_x, m_texture_y, m_texture_scale);
	}
	
	if (m_rects && m_nrects > 0) {
		int n = 0;
		
		if (properties->display_collision_box && m_nrect_collision > 0) {
			render_boxes(BOX_COLLISION, m_rects + n, m_nrect_collision, properties->display_solid_boxes);
		}
		
		n += m_nrect_collision;
		
		if (properties->display_hit_box && m_nrect_hit > 0) {
			render_boxes(BOX_HIT, m_rects + n, m_nrect_hit, properties->display_solid_boxes);
		}
		
		n += m_nrect_hit;
		
		if (properties->display_attack_box && m_nrect_attack > 0) {
			render_boxes(BOX_ATTACK, m_rects + n, m_nrect_attack, properties->display_solid_boxes);
		}
		
		n += m_nrect_attack;
		
		if (properties->display_clash_box && m_nrect_clash > 0) {
			render_boxes(BOX_CLASH, m_rects + n, m_nrect_clash, properties->display_solid_boxes);
		}
		
		n += m_nrect_clash;
		
		// display misc?
	}
}

bool Clone::in_box(int x, int y) {
	if (!m_rects || !m_nrects) {
		return 0;
	}
	
	int count = m_nrect_collision + m_nrect_attack + m_nrect_hit + m_nrect_clash;
	for (int i = 0; i < count; ++i) {
		rect_t *rect = &m_rects[i];
		
		if (rect->x2 > x && x > rect->x1
		    && rect->y2 > y && y > rect->y1) {
			return 1;
		}
	}
	
	return 0;
}

void Clone::free() {
	if (m_texture) {
		delete m_texture;
		m_texture = 0;
	}
	
	if (m_rects) {
		delete m_rects;
		m_rects = 0;
	}
	m_nrects = 0;
}

Clone::Clone() {
	m_texture = 0;
	
	m_rects = 0;
	m_nrects = 0;
}

Clone::~Clone() {
	free();
}

