#ifndef FRAMEDISPLAY_H
#define FRAMEDISPLAY_H

// external
class Clone;

enum FrameDisplayCommand {
	COMMAND_CHARACTER_NEXT,
	COMMAND_CHARACTER_PREV,
	COMMAND_CHARACTER_SET,
	COMMAND_SEQUENCE_NEXT,
	COMMAND_SEQUENCE_PREV,
	COMMAND_SEQUENCE_SET,
	COMMAND_FRAME_NEXT,
	COMMAND_FRAME_PREV,
	COMMAND_FRAME_SET,
	COMMAND_SUBFRAME_NEXT,
	COMMAND_SUBFRAME_PREV,
	COMMAND_SUBFRAME_SET,
	COMMAND_PALETTE_NEXT,
	COMMAND_PALETTE_PREV,
	COMMAND_PALETTE_SET
};

struct RenderProperties {
	bool		display_sprite;

	bool		display_solid_boxes;

	bool		display_collision_box;
	bool		display_hit_box;
	bool		display_attack_box;
	bool		display_clash_box;
};

class FrameDisplay {
protected:
	bool		m_initialized;

	int		m_character;
	int		m_sequence;
	int		m_state;
	int		m_frame;
	int		m_palette;
public:
	virtual const char *get_character_long_name(int n);
	virtual const char *get_character_name(int n);
	int		get_character();

	int		get_sequence();
	virtual int	get_sequence_count();
	virtual bool	has_sequence(int n);
	virtual const char *get_sequence_name(int n);
	virtual const char *get_sequence_move_name(int n, int *dmeter);

	int		get_frame();
	virtual int	get_frame_count();
	virtual int	get_subframe();
	virtual int	get_subframe_count();
	int		get_palette();

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

	virtual void	free();

			FrameDisplay();
	virtual		~FrameDisplay();
};

#endif
