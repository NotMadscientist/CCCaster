#include "texture.h"

#include <cstdio>

#include <png.h>

#include <gl.h>

bool Texture::init_gl() {
	if (m_8bpp) {
		return 0; // unsupported
	}
	
	GLuint tex_id;
	
	glGenTextures(1, &tex_id);
	
	m_gl_texture_id = tex_id;
	
	glBindTexture(GL_TEXTURE_2D, m_gl_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
		m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		m_pixels);
	
	m_has_gl = 1;
	
	return 1;
}

void Texture::filter(bool bilinear) {
	if (!m_has_gl) {
		if (!init_gl()) {
			return;
		}
	}
	
	glBindTexture(GL_TEXTURE_2D, m_gl_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Texture::draw(int x, int y, int size) {
	if (!m_initialized) {
		return;
	}
	
	if (!m_has_gl) {
		if (!init_gl()) {
			return;
		}
	}
	
	bool matrixed = 0;
	
	if (m_rotate || m_scale || m_offset_x || m_offset_y) {
		matrixed = 1;
		glPushMatrix();
	}
	
	if (m_mode & 1) {
		glTranslatef(m_offset_x, m_offset_y, 0.0);
	}

	if (m_rotate) {
		if (m_z_rotation != 0.0) {
			glRotatef(m_z_rotation * 360.0, 0.0, 0.0, 1.0);
		}
		if (m_x_rotation != 0.0) {
			glRotatef(m_x_rotation * 360.0, 1.0, 0.0, 0.0);
		}
		if (m_y_rotation != 0.0) {
			glRotatef(m_y_rotation * 360.0, 0.0, 1.0, 0.0);
		}
	}

	if (m_scale) {
		glScalef(m_scale_x, m_scale_y, 1.0);
	}
	
	if (!(m_mode & 1)) {
		x += m_offset_x;
		y += m_offset_y;
	}
	
	if (m_blend_mode > 0) {
		if (m_blend_mode >= 2) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			// FIXME m_blend_mode 3 = glBlendEquation(GL_FUNC_REVERSE_SUBTRACT)
		}
		glColor4f(m_r, m_g, m_b, m_alpha);
	} else {
		glColor4f(1.0, 1.0, 1.0, 1.0);
	}
	
	glBindTexture(GL_TEXTURE_2D, m_gl_texture_id);
	glEnable(GL_TEXTURE_2D);
	
	int w = m_width * size;
	int h = m_height * size;
	
	int x1 = x;
	int y1 = y;
	int x2 = x+w;
	int y2 = y+h;
	
	if (m_mode & 2) {
		x2 = x - w;
	}
	
	glBegin(GL_QUADS);
	glTexCoord2f(0.0,0.0);
	glVertex2i(x1,y1);
	glTexCoord2f(1.0,0.0);
	glVertex2i(x2,y1);
	glTexCoord2f(1.0,1.0);
	glVertex2i(x2,y2);
	glTexCoord2f(0.0,1.0);
	glVertex2i(x1,y2);
	glEnd();
	
	glDisable(GL_TEXTURE_2D);
	
	if (m_blend_mode == 2) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (matrixed) {
		glPopMatrix();
	}
}

bool Texture::save_to_png(const char *filename, unsigned int *palette) {
	if (!m_initialized) {
		return 0;
	}
	
	if (m_8bpp && !palette) {
		return 0;
	}
	
	FILE *file;
	
	file = fopen(filename, "wb");
	if (!file) {
		return 0;
	}
	
	png_struct *png_write;
	png_info *png_info;

	png_write = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_info = png_create_info_struct(png_write);

	png_init_io(png_write, file);

	png_set_compression_level(png_write, Z_BEST_COMPRESSION);
	png_set_IHDR(png_write, png_info, m_width, m_height, 8,
		m_8bpp?PNG_COLOR_TYPE_PALETTE:PNG_COLOR_TYPE_RGB_ALPHA,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);
	
	if (m_8bpp) {
		png_color png_pal[256];
		png_byte trans[256];
		int ntrans = 0;
		
		unsigned char *p = (unsigned char *)palette;
		for (int i = 0; i < 256; ++i) {
			png_pal[i].red = *p++;
			png_pal[i].green = *p++;
			png_pal[i].blue = *p++;
			
			if (*p == 0) {
				trans[ntrans] = i;
				++ntrans;
			}
			
			++p;
		}
		
		png_set_PLTE(png_write, png_info, png_pal, 256);
		
		if (ntrans > 0) {
			png_set_tRNS(png_write, png_info, trans, ntrans, 0);
		}
	}
	
	png_byte *row_pointers[m_height];
	unsigned char *p;
	
	p = m_pixels;
	if (m_8bpp) {
		for (int i = 0; i < m_height; ++i) {
			row_pointers[i] = (png_byte *)p;
			p += m_width;
		}
	} else {
		for (int i = 0; i < m_height; ++i) {
			row_pointers[i] = (png_byte *)p;
			p += m_width*4;
		}
	}
	
	png_write_info(png_write, png_info);
	png_write_image(png_write, row_pointers);
	png_write_end(png_write, NULL);
	png_destroy_write_struct(&png_write, &png_info);

	fclose(file);
		
	return 1;
}

void Texture::offset(int x, int y) {
	m_offset_x = x;
	m_offset_y = y;
}

void Texture::blend_mode(int mode) {
	m_blend_mode = mode;
}

void Texture::color(float r, float g, float b) {
	m_r = r;
	m_g = g;
	m_b = b;
}

void Texture::rotate_clear() {
	m_rotate = 0;
	m_z_rotation = 0;
	m_x_rotation = 0;
	m_y_rotation = 0;
}

void Texture::rotate_z(float value) {
	m_rotate = 1;
	m_z_rotation = value;
}

void Texture::rotate_x(float value) {
	m_rotate = 1;
	m_x_rotation = value;
}

void Texture::rotate_y(float value) {
	m_rotate = 1;
	m_y_rotation = value;
}

void Texture::scale_clear() {
	m_scale = 0;
}

void Texture::scale(float x, float y) {
	m_scale = 1;
	m_scale_x = x;
	m_scale_y = y;
}

void Texture::alpha(float alpha) {
	m_alpha = alpha;
}

void Texture::special_mode(int mode) {
	m_mode = mode;
}

bool Texture::init(unsigned char *pixels, int width, int height, bool is_8bpp) {
	if (m_initialized) {
		return 0;
	}
	
	m_width = width;
	m_height = height;
	m_pixels = pixels;
	m_offset_x = 0;
	m_offset_y = 0;
	
	m_blend_mode = 0;
	
	m_rotate = 0;
	m_scale = 0;
	
	m_8bpp = is_8bpp;
	
	m_initialized = 1;
	
	return 1;
}

void Texture::free() {
	if (m_has_gl) {
		glDeleteTextures(1, &m_gl_texture_id);
		
		m_has_gl = 0;
	}
	
	if (m_pixels) {
		delete[] m_pixels;
	}
	m_pixels = 0;
	
	m_width = 0;
	m_height = 0;
	
	m_offset_x = 0;
	m_offset_y = 0;
	
	m_blend_mode = 0;
	m_alpha = 1.0;
	
	m_rotate = 0;
	m_z_rotation = 0;
	m_x_rotation = 0;
	m_y_rotation = 0;
	m_scale = 0;
	
	m_8bpp = 0;
	
	m_initialized = 0;
}

Texture::Texture() {
	m_initialized = 0;
	
	m_width = 0;
	m_height = 0;
	
	m_offset_x = 0;
	m_offset_y = 0;
	
	m_pixels = 0;
	
	m_blend_mode = 0;
	
	m_r = 1.0;
	m_g = 1.0;
	m_b = 1.0;
	m_alpha = 1.0;
	
	m_rotate = 0;
	m_z_rotation = 0;
	m_x_rotation = 0;
	m_y_rotation = 0;
	m_scale = 0;
	
	m_gl_texture_id = 0;
	m_has_gl = 0;
	
	m_mode = 0;
	
	m_8bpp = 0;
}

Texture::~Texture() {
	free();
}

