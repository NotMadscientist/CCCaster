// mbaacc_pack:
//
// File loader for MBAACC PC edition.

#include "mbaacc_pack.h"

#include <cstring>

// decryption function. Only applies to first 4096 bytes of data.
void decrapt(unsigned char *data, unsigned int size,
		unsigned int xorkey, unsigned int xormod) {
	union {
		unsigned int key;
		struct {
			unsigned char a;
			unsigned char b;
			unsigned char c;
			unsigned char d;
		} b;
	} key_a;
	unsigned int key_b;
	
	key_a.key = xorkey;
	
	key_b = xormod & 0xff;
	if (key_b == 0) {
		key_b = 1;
	}
	
	if (size > 4096) {
		size = 4096;
	}
	
	unsigned int *p = (unsigned int *)data;
	size = (size + 3) / 4;
	for (unsigned int i = 0; i < size; ++i) {
		*p++ ^= key_a.key;
		
		key_a.b.a += key_b;
		key_a.b.b += key_b;
		key_a.b.c += key_b;
		key_a.b.d += key_b;
	}
}

bool MBAACC_Pack::open_pack(const char *filename) {
	if (m_file) {
		return 0;
	}
	
	FILE *f;
	
	f = fopen(filename, "rb");
	if (!f) {
		return 0;
	}
	
	struct header_t {
		unsigned char string[16];
		unsigned int flag;
		unsigned int xor_key;
		unsigned int table_size;
		unsigned int data_size;
		unsigned int folder_count;
		unsigned int file_count;
		unsigned int unknown[3];
	} header;
	
	if (fread(&header, sizeof(header), 1, f) <= 0) {
		fclose(f);
		return 0;
	}
	
	if (memcmp(header.string, "FilePacHeaderA", 14)) {
		fclose(f);
		return 0;
	}
	
	if (header.folder_count > 10000 || header.file_count > 10000) {
		fclose(f);
		return 0;
	}
	
	// read the folder/file index in
	FolderIndex *folder_index = new FolderIndex[header.folder_count];
	
	fread(folder_index, header.folder_count, sizeof(FolderIndex), f);
	m_data_folder_id = 32768;
	
	for (unsigned int i = 0; i < header.folder_count; ++i) {
		decrapt(folder_index[i].filename, 256, header.xor_key, folder_index[i].size);
		
		if (!strcmp((char *)folder_index[i].filename, ".\\data")) {
			m_data_folder_id = i;
		}
	}
	
	if (m_data_folder_id == 32768) {
		delete[] folder_index;
		
		fclose(f);
		
		return 0;
	}
	
	FileIndex *file_index = new FileIndex[header.file_count];
	fread(file_index, header.file_count, sizeof(FileIndex), f);
	
	for (unsigned int i = 0; i < header.file_count; ++i) {
		decrapt(file_index[i].filename, 32, header.xor_key, file_index[i].size);
	}
	
	// finish up!
	
	m_data_start = header.table_size;
	m_xor_key = header.xor_key;
	
	m_folder_index = folder_index;
	m_file_index = file_index;
	m_file = f;
	
	return 1;
}

void MBAACC_Pack::close_pack() {
	if (!m_file) {
		return;
	}
	
	fclose(m_file);
	m_file = 0;
	
	if (m_folder_index) {
		delete[] m_folder_index;
		m_folder_index = 0;
	}
	m_folder_count = 0;
	
	if (m_file_index) {
		delete[] m_file_index;
		m_file_index = 0;
	}
	m_file_count = 0;
}

bool MBAACC_Pack::read_file(const char *filename, char **dest, unsigned int *dsize) {
	// hacky, but it's in the middle so it doesn't matter.
	unsigned int n = m_folder_index[m_data_folder_id].file_start_id;
	
	unsigned int folder = m_data_folder_id + 1;
	unsigned int n_end;
	
	do {
		if (folder == m_folder_count) {
			n_end = m_file_count;
		} else {
			n_end = m_folder_index[folder++].file_start_id;
		}
	} while (n_end == 0);
	
	while (n < n_end) {
		if (!strcasecmp((char *)m_file_index[n].filename, filename)) {
			// Found it. Read it in.
			unsigned char *data = new unsigned char[m_file_index[n].size + 3];
			
			fseek(m_file, m_data_start + m_file_index[n].pos, SEEK_SET);
			
			int count = fread(data, m_file_index[n].size, 1, m_file);
			
			if (count >= 1) {
				decrapt(data, m_file_index[n].size, m_xor_key, 0x03);
				
				data[m_file_index[n].size] = '\0';
				
				*dest = (char *)data;
				*dsize = m_file_index[n].size;
				
				return 1;
			}
			
			return 0;
		}
		
		n += 1;
	}
	
	return 0;
}

MBAACC_Pack::MBAACC_Pack() {
	m_file = 0;
	
	m_folder_index = 0;
	m_folder_count = 0;
	
	m_file_index = 0;
	m_file_count = 0;
}

MBAACC_Pack::~MBAACC_Pack() {
	close_pack();
}

