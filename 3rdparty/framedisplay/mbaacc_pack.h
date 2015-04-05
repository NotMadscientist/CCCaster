#ifndef MBAACC_PACK_H
#define MBAACC_PACK_H

#include <cstdio>

class MBAACC_Pack {
private:
	FILE		*m_file;
	
	struct FolderIndex {
		unsigned int pos;
		unsigned int file_start_id;
		unsigned int size;
		unsigned char filename[256];
	};
	struct FileIndex {
		unsigned int pos;
		unsigned int unknown;
		unsigned int size;
		unsigned char filename[32];
	};
	
	FolderIndex	*m_folder_index;
	unsigned int	m_folder_count;
	
	unsigned int	m_data_folder_id;
	
	unsigned int	m_data_start;

	unsigned int	m_xor_key;
	
	FileIndex	*m_file_index;
	int		m_file_count;
public:
	bool		open_pack(const char *filename);
	void		close_pack();
	
	bool		read_file(const char *filename, char **dest, unsigned int *size);
	
			MBAACC_Pack();
			~MBAACC_Pack();
};

#endif
