#ifndef MBAACC_FRAMEDISPLAY_H
#define MBAACC_FRAMEDISPLAY_H

#include <string>
#include <list>
#include <map>

#include "framedisplay.h"

#include "texture.h"

#include "mbaacc_pack.h"

// ************************************************** mbaacc_cg.cpp

struct MBAACC_CG_Image;
struct MBAACC_CG_Alignment;

class MBAACC_CG {
protected:
	bool				m_loaded;

	char				*m_data;
	unsigned int			m_data_size;

	const unsigned int		*m_indices;

	unsigned int			m_nimages;

	const MBAACC_CG_Alignment	*m_align;
	unsigned int			m_nalign;

	struct ImageCell {
		unsigned int		start;
		unsigned int		width;
		unsigned int		height;
		unsigned int		offset;
		unsigned short		type_id;
		unsigned short		bpp;
	};

	struct Image {
		ImageCell		cell[256];
	};

	Image				*m_image_table;
	unsigned int			m_image_count;

	void			copy_cells(
					const MBAACC_CG_Image *image,
					const MBAACC_CG_Alignment *align,
					unsigned char *pixels,
					unsigned int x1,
					unsigned int y1,
					unsigned int width,
					unsigned int height,
					unsigned int *palette,
					bool is_8bpp);

	void			build_image_table();

	const MBAACC_CG_Image	*get_image(unsigned int n);
public:
	bool			load(MBAACC_Pack *pack, const char *name);

	void			free();

	const char		*get_filename(unsigned int n);

	Texture			*draw_texture(unsigned int n,
					unsigned int *palette, bool to_pow2,
					bool draw_8bpp = 0);

	int			get_image_count();

				MBAACC_CG();
				~MBAACC_CG();
};

// ************************************************** mbaacc_framedata.cpp

// cleverly organized to be similar to MBAC's frame data.

struct MBAACC_Hitbox {
	short x1, y1, x2, y2;
};

struct MBAACC_Frame_AF {
	// rendering data
	bool		active;

	int		frame;
	int		frame_unk;

	int		offset_y;
	int		offset_x;

	int		duration;
	int		AFF;

	int		blend_mode;

	unsigned char	alpha;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;

	float		z_rotation;
	float		y_rotation;
	float		x_rotation;

	bool		has_zoom;
	float		zoom_x;
	float		zoom_y;

	int		AFJP;
};

struct MBAACC_Frame_AS {
	// state data
	int		speed_flags;
	int		speed_horz;
	int		speed_vert;
	int		accel_horz;
	int		accel_vert;

	int		ASMV;

	int		stand_state;

	int		cancel_flags;
};

struct MBAACC_Frame_AT {
	bool		active;

	int		guard_flags;

	int		proration;
	int		proration_type;

	int		damage;
	int		red_damage;
	int		dmg_unknown;
	int		circuit_gain;
};

struct MBAACC_Frame_EF {
	int		command;
	int		parameter;
	int		values[12];
};

struct MBAACC_Frame_IF {
	int		command;
	int		values[12];
};

struct MBAACC_Frame {
	MBAACC_Frame_AF	AF;

	MBAACC_Frame_AS	*AS;
	MBAACC_Frame_AT	*AT;

	MBAACC_Frame_EF	*EF[8];
	MBAACC_Frame_IF	*IF[8];

	MBAACC_Hitbox	*hitboxes[33];
};

struct MBAACC_Sequence {
	// sequence property data
	std::string	name;

	bool		is_move;
	std::string	move_name;
	int		move_meter;

	int		subframe_count;

	bool		initialized;

	char		*data;

	MBAACC_Frame	*frames;
	unsigned int	nframes;

	MBAACC_Hitbox	*hitboxes;
	unsigned int	nhitboxes;

	MBAACC_Frame_AT	*AT;
	unsigned int	nAT;

	MBAACC_Frame_AS	*AS;
	unsigned int	nAS;

	MBAACC_Frame_EF	*EF;
	unsigned int	nEF;

	MBAACC_Frame_IF	*IF;
	unsigned int	nIF;
};

class MBAACC_FrameData {
private:
	MBAACC_Sequence	**m_sequences;
	unsigned int	m_nsequences;

	bool		m_loaded;
public:
	bool		load(MBAACC_Pack *pack, const char *filename);

	bool		load_move_list(MBAACC_Pack *pack, const char *filename);

	int		get_sequence_count();

	MBAACC_Sequence	*get_sequence(int n);

	void		free();

		MBAACC_FrameData();
		~MBAACC_FrameData();
};


// ************************************************** mbaacc_character.cpp

class MBAACC_Character {
protected:
	bool		m_loaded;

	MBAACC_FrameData	m_framedata;

	char		*m_name;

	MBAACC_CG	m_cg;

	unsigned int	**m_palettes;

	int		m_active_palette;

	Texture		*m_texture;
	int		m_last_sprite_id;

	bool		do_sprite_save(int id, const char *filename);

	MBAACC_Frame *	get_frame(int seq_id, int fr_id);

	void		set_render_properties(const MBAACC_Frame *frame, Texture *texture);
public:
	bool		load(MBAACC_Pack *pack, const char *name, int sub_type);

	void		load_graphics(MBAACC_Pack *pack);
	void		unload_graphics();

	void		render(const RenderProperties *properties, int seq, int frame);
	Clone		*make_clone(int seq_id, int fr_id);

	void		flush_texture();

	void		render_frame_properties(bool detailed, int scr_width, int scr_height, int seq, int frame);

	void		set_palette(int n);

	int		get_sequence_count();
	bool		has_sequence(int n);
	const char *	get_sequence_name(int n);
	const char *	get_sequence_move_name(int n, int *meter);

	int		get_frame_count(int seq_id);
	int		get_subframe_count(int seq_id);
	int		get_subframe_length(int seq_id, int fr_id);
	int		count_subframes(int seq_id, int fr_id);

	int		find_sequence(int seq_id, int direction);
	int		find_frame(int seq_id, int fr_id);

	const char *	get_current_sprite_filename(int seq_id, int fr_id);
	bool		save_current_sprite(const char *filename, int seq_id, int fr_id);
	int		save_all_character_sprites(const char *directory);

	void		free_frame_data();
	void		free_graphics();
	void		free();

			MBAACC_Character();
			~MBAACC_Character();

	const unsigned int **get_palette_data() const { return ( const unsigned int ** ) m_palettes; }
	unsigned int **get_palette_data() { return m_palettes; }
};

// ************************************************** mbaacc_framedisplay.cpp

class MBAACC_FrameDisplay : public FrameDisplay {
protected:
	MBAACC_Pack	m_pack;

	MBAACC_Character	m_character_data;

	int		m_subframe_base;
	int		m_subframe_next;
	int		m_subframe;

	void		set_active_character(int n);
	void		set_palette(int n);
	void		set_sequence(int n);
	void		set_frame(int n);
public:
	virtual const char *get_character_long_name(int n);
	virtual const char *get_character_name(int n);
	int get_character_count() const;
	int get_character_index(int n);
	virtual int	get_sequence_count();
	virtual bool	has_sequence(int n);
	virtual const char *get_sequence_name(int n);
	virtual const char *get_sequence_move_name(int n, int *meter);
	virtual int	get_frame_count();
	virtual int	get_subframe();
	virtual int	get_subframe_count();

	virtual void	render(const RenderProperties *properties);
	virtual Clone	*make_clone();

	virtual void	flush_texture();

	virtual void	render_frame_properties(bool detailed, int scr_width, int scr_height);

	virtual void	command(FrameDisplayCommand command, const void *param);

	virtual const char *get_current_sprite_filename();
	virtual bool	save_current_sprite(const char *filename);
	virtual int	save_all_character_sprites(const char *directory);

	virtual bool	init();
	virtual bool	init(const char *filename);

	virtual	void	free();

			MBAACC_FrameDisplay();
	virtual		~MBAACC_FrameDisplay();

	const unsigned int **get_palette_data() const { return m_character_data.get_palette_data(); }
	unsigned int **get_palette_data() { return m_character_data.get_palette_data(); }
};

#endif
