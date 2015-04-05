#ifndef TEXTURE_H
#define TEXTURE_H

class Texture {
private:
	bool		m_initialized;
	
	int		m_width;
	int		m_height;
	
	int		m_offset_x;
	int		m_offset_y;
	
	unsigned char	*m_pixels;
	
	unsigned int	m_gl_texture_id;
	bool		m_has_gl;
	
	int		m_blend_mode;
	float		m_r, m_g, m_b;
	float		m_alpha;
	
	bool		m_rotate;
	float		m_z_rotation;
	float		m_x_rotation;
	float		m_y_rotation;
	
	bool		m_scale;
	float		m_scale_x;
	float		m_scale_y;
	
	int		m_mode;
	
	bool		m_8bpp;
	
	bool	init_gl();
public:
	bool	init(unsigned char *pixels, int width, int height, bool is_8bpp);

	void	blend_mode(int mode);
	void	color(float r, float g, float b);
	void	alpha(float alpha);
	
	void	rotate_clear();
	void	rotate_z(float value);
	void	rotate_x(float value);
	void	rotate_y(float value);
	
	void	scale_clear();
	void	scale(float x, float y);
	
	void	draw(int x, int y, int size);
	
	void	offset(int x, int y);
	
	void	filter(bool bilinear);
	
	void	special_mode(int mode);
	
	void	free();
	
	bool	save_to_png(const char *filename, unsigned int *palette = 0);
	
		Texture();
		~Texture();
};

#endif
