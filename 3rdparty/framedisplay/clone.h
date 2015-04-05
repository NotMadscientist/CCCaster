#ifndef CLONE_H
#define CLONE_H

#include "texture.h"
#include "render.h"
#include "framedisplay.h"

struct CloneHitbox {
	BoxType		type;
	rect_t		rect;
};

class Clone {
private:
	Texture		*m_texture;
	int		m_texture_x;
	int		m_texture_y;
	int		m_texture_scale;
	
	rect_t		*m_rects;
	int		m_nrect_collision;
	int		m_nrect_hit;
	int		m_nrect_attack;
	int		m_nrect_clash;
	int		m_nrect_default;
	
	int		m_nrects;
public:
	void		init_texture(Texture *m_sprite, int x, int y, int scale);
	void		init_hitboxes(CloneHitbox *hitboxes, int nhitboxes);
	
	void		free();
	
	void		render(const RenderProperties *properties);
	
	bool		in_box(int x, int y);
	
			Clone();
			~Clone();
};

#endif
