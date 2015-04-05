#ifndef RENDER_H
#define RENDER_H

struct rect_t {
    short x1, y1, x2, y2;
};

enum BoxType {
	BOX_DEFAULT = 0,
	BOX_COLLISION = 1,
	BOX_HIT = 2,
	BOX_ATTACK = 3,
	BOX_CLASH = 4
};

extern void render_font_init();
extern void render_color(char color);
extern int render_text(int x, int y, const char *str, bool colorflag = 1);
//extern int render_shaded_text(int x, int y, const char *str);
extern int render_shaded_text(int x, int y, const char *string, ...);
extern int render_line(int x, int y, int w, char color = '\0');
extern int render_shaded_line(int x, int y, int w, char color = '\0');

extern void render_boxes(BoxType box_type, rect_t *rects, int nrects, bool solid);

#endif
