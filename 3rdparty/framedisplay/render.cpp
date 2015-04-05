#include <gl.h>

#include <cstdarg>
#include <cstdlib>
#include <cstdio>

#include "render.h"

void render_font_init() {
}

void render_font_clear() {
}

void render_color(char ch) {
	switch (ch) {
	case 'w':
	case '.':
		glColor4f(1.0, 1.0, 1.0, 1.0);
		break;
	case 'W':
		glColor4f(0.7, 0.7, 0.7, 1.0);
		break;
	case 'R':
		glColor4f(1.0, 0.0, 0.0, 1.0);
		break;
	case 'r':
		glColor4f(1.0, 0.7, 0.7, 1.0);
		break;
	case 'G':
		glColor4f(0.0, 1.0, 0.0, 1.0);
		break;
	case 'g':
		glColor4f(0.7, 1.0, 0.7, 1.0);
		break;
	case 'B':
		glColor4f(0.6, 0.6, 1.0, 1.0);
		break;
	case 'b':
		glColor4f(0.8, 0.8, 1.0, 1.0);
		break;
	case 'M':
		glColor4f(1.0, 0.0, 1.0, 1.0);
		break;
	case 'm':
		glColor4f(1.0, 0.7, 1.0, 1.0);
		break;
	case 'Y':
		glColor4f(1.0, 1.0, 0.0, 1.0);
		break;
	case 'y':
		glColor4f(1.0, 1.0, 0.7, 1.0);
		break;
	case 'C':
		glColor4f(0.0, 1.0, 1.0, 1.0);
		break;
	case 'c':
		glColor4f(0.8, 1.0, 1.0, 1.0);
		break;
	case 'O':
		glColor4f(1.0, 0.5, 0.0, 1.0);
		break;
	case 'o':
		glColor4f(1.0, 0.7, 0.5, 1.0);
		break;
	case 'D':
		glColor4f(1.0, 1.0, 1.0, 0.5);
		break;
	case 'd':
		glColor4f(1.0, 1.0, 1.0, 0.3);
		break;
	}
}

int render_text(int dx, int dy, const char *string, bool colorflag) {
	return 0;
}

int render_shaded_text_int(int x, int y, const char *string) {
	return 0;
}

int render_shaded_text(int x, int y, const char *string, ...) {
	return 0;
}

int render_line(int x, int y, int w, char color) {
	y += 4;
	glBegin(GL_LINES);
	glVertex2i(x, y);
	glVertex2i(x+w, y);
	glEnd();

	return 4;
}

int render_shaded_line(int x, int y, int w, char color) {
	glColor4f(0.0, 0.0, 0.0, 1.0);
	render_line(x, y, w);

	if (color != '\0') {
		render_color(color);
	} else {
		glColor4f(1.0, 1.0, 1.0, 1.0);
	}
	return render_line(x, y, w);
}

// *************************************************** BOX CODE

// this needs rewriting.
struct node {
	int x1, y1, x2, y2;
	node *next;
};

static node *create_box(rect_t *rects, int count)
{
	node *first = NULL;
	node *cur = NULL;
	int error;

	for (int i = 0; i < count; ++i) {
		rect_t *rect;
		rect = &rects[i];

		node *tmp = new node;
		tmp->next = NULL;
		if (cur != NULL) {
			cur->next = tmp;
		}
		if (first == NULL) {
			first = tmp;
		}
		cur = tmp;
		cur->x1 = rect->x1;
		cur->x2 = rect->x2;
		cur->y1 = rect->y1;
		cur->y2 = rect->y2;
	}

	if (!first) {
		return 0;
	}

	do {
		error = 0;
		cur = first;
		while (cur->next != NULL) {
			node *tmp = NULL;
			do {
				if (tmp == NULL) {
					tmp = cur->next;
				} else {
					tmp = tmp->next;
				}
				if ((cur->x1 < tmp->x1) && (cur->x2 > tmp->x1)) {
					node *new1 = new node;
					node *new2 = new node;
					new1->x1 = cur->x1;
					new1->x2 = tmp->x1;
					new2->x1 = tmp->x1;
					new2->x2 = cur->x2;
					new1->y1 = cur->y1;
					new1->y2 = cur->y2;
					new2->y1 = cur->y1;
					new2->y2 = cur->y2;
					new1->next = new2;
					new2->next = cur->next;
					node *tmp2 = first;
					if (cur == first) {
						first = new1;
						delete tmp2;
					} else {
						while (tmp2->next != cur) {
							tmp2 = tmp2->next;
						}
						tmp2->next = new1;
						delete cur;
						cur = first;
					}
					error = 1;
					break;
				}
				if ((cur->x1 < tmp->x2) && (cur->x2 > tmp->x2)) {
					node *new1 = new node;
					node *new2 = new node;
					new1->x1 = cur->x1;
					new1->x2 = tmp->x2;
					new2->x1 = tmp->x2;
					new2->x2 = cur->x2;
					new1->y1 = cur->y1;
					new1->y2 = cur->y2;
					new2->y1 = cur->y1;
					new2->y2 = cur->y2;
					new1->next = new2;
					new2->next = cur->next;
					node *tmp2 = first;
					if (cur == first) {
						first = new1;
						delete tmp2;
					} else {
						while (tmp2->next != cur) {
							tmp2 = tmp2->next;
						}
						tmp2->next = new1;
						delete cur;
						cur = first;
					}
					error = 1;
					break;
				}
				if ((cur->y1 < tmp->y1) && (cur->y2 > tmp->y1)) {
					node *new1 = new node;
					node *new2 = new node;
					new1->y1 = cur->y1;
					new1->y2 = tmp->y1;
					new2->y1 = tmp->y1;
					new2->y2 = cur->y2;
					new1->x1 = cur->x1;
					new1->x2 = cur->x2;
					new2->x1 = cur->x1;
					new2->x2 = cur->x2;
					new1->next = new2;
					new2->next = cur->next;
					node *tmp2 = first;
					if (cur == first) {
						first = new1;
						delete tmp2;
					} else {
						while (tmp2->next != cur) {
							tmp2 = tmp2->next;
						}
						tmp2->next = new1;
						delete cur;
						cur = first;
					}
					error = 1;
					break;
				}
				if ((cur->y1 < tmp->y2) && (cur->y2 > tmp->y2)) {
					node *new1 = new node;
					node *new2 = new node;
					new1->y1 = cur->y1;
					new1->y2 = tmp->y2;
					new2->y1 = tmp->y2;
					new2->y2 = cur->y2;
					new1->x1 = cur->x1;
					new1->x2 = cur->x2;
					new2->x1 = cur->x1;
					new2->x2 = cur->x2;
					new1->next = new2;
					new2->next = cur->next;
					node *tmp2 = first;
					if (cur == first) {
						first = new1;
						delete tmp2;
					} else {
						while (tmp2->next != cur) {
							tmp2 = tmp2->next;
						}
						tmp2->next = new1;
						delete cur;
						cur = first;
					}
					error = 1;
					break;
				}
				if ((cur->x1 >= tmp->x1) && (cur->x2 <= tmp->x2)
				    && (cur->y1 >= tmp->y1) && (cur->y2 <= tmp->y2)) {
					node *tmp2 = first;
					if (cur == first) {
						first = first->next;
						delete tmp2;
					} else {
						while (tmp2->next != cur) {
							tmp2 = tmp2->next;
						}
						tmp2->next = cur->next;
						tmp2 = cur;
						cur = first;
						delete tmp2;
					}
					error = 1;
					break;
				}
			} while (tmp->next != NULL);

			if (error == 1) {
				break;
			}
			cur = cur->next;
		}
	} while (error != 0);
	return first;
}

void render_boxes(BoxType box_type, rect_t *rects, int nrects, bool solid) {
	if (solid) {
		switch (box_type) {
		case BOX_DEFAULT:
			glColor4f(1.0, 0.3, 1.0, 0.3);
			break;
		case BOX_COLLISION:
			glColor4f(0.8, 0.8, 0.8, 0.25);
			break;
		case BOX_HIT:
			glColor4f(0.3, 1.0, 0.3, 0.3);
			break;
		case BOX_ATTACK:
			glColor4f(1.0, 0.3, 0.3, 0.3);
			break;
		case BOX_CLASH:
			glColor4f(1.0, 1.0, 0.3, 0.3);
			break;
		default:
			return;
		}

		if (nrects == 1) {
			glBegin(GL_QUADS);
			glVertex2i(rects[0].x1, rects[0].y1);
			glVertex2i(rects[0].x2, rects[0].y1);
			glVertex2i(rects[0].x2, rects[0].y2);
			glVertex2i(rects[0].x1, rects[0].y2);
			glEnd();
		} else {
			glBegin(GL_QUADS);

			node *node = 0;

			do {
				if (node == 0) {
					node = create_box(rects, nrects);

					if (!node) {
						break;
					}
				} else {
					struct node *nodetemp = node;
					node = node->next;

					delete nodetemp;
				}
				glVertex2f(node->x1, node->y1);
				glVertex2f(node->x2, node->y1);
				glVertex2f(node->x2, node->y2);
				glVertex2f(node->x1, node->y2);

			} while (node->next != 0);
			delete node;

			glEnd();
		}
	}

	switch (box_type) {
	case BOX_DEFAULT:
		glColor4f(1.0, 0.3, 1.0, 1.0);
		break;
	case BOX_COLLISION:
		glColor4f(0.9, 0.9, 0.9, 1.0);
		break;
	case BOX_HIT:
		glColor4f(0.3, 1.0, 0.3, 1.0);
		break;
	case BOX_ATTACK:
		glColor4f(1.0, 0.3, 0.3, 1.0);
		break;
	case BOX_CLASH:
		glColor4f(1.0, 1.0, 0.3, 1.0);
		break;
	default:
		return;
	}

	for (int i = 0; i < nrects; ++i) {
		glBegin(GL_LINE_LOOP);

		glVertex2i(rects[i].x1, rects[i].y1);
		glVertex2i(rects[i].x2, rects[i].y1);
		glVertex2i(rects[i].x2, rects[i].y2);
		glVertex2i(rects[i].x1, rects[i].y2);

		glEnd();
	}
}
