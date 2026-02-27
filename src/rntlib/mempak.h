#ifndef _mempak_h__
#define _mempak_h__

/* Size of one bank (standard Controller Pak) */
#define MEMPAK_MEM_SIZE		0x8000

/* Multi-bank sizes in bytes — matches Datel / ares conventions */
#define MEMPAK_SIZE_32K		0x8000		/* 1  bank  — Standard Nintendo Controller Pak */
#define MEMPAK_SIZE_128K	0x20000		/* 4  banks — Datel 1Meg Memory Card           */
#define MEMPAK_SIZE_512K	0x80000		/* 16 banks — Datel 4Meg Memory Card           */
#define MEMPAK_SIZE_1984K	0x1F0000	/* 62 banks — Maximum possible                 */

#define MEMPAK_BANKS_MAX	62
#define MEMPAK_BANK_SIZE	MEMPAK_MEM_SIZE

/* Derive bank count from a total size */
#define MEMPAK_BANK_COUNT(size)   ((unsigned int)(size) / MEMPAK_BANK_SIZE)

#define MEMPAK_NUM_NOTES	16

#define MAX_NOTE_COMMENT_SIZE	257 /* including NUL terminator */

#define MPK_FORMAT_INVALID	0
#define MPK_FORMAT_MPK		1
#define MPK_FORMAT_MPK4		2 /* MPK + 3 × 32 KiB padding */
#define MPK_FORMAT_N64		3

typedef struct mempak_structure
{
	unsigned char *data;        /* dynamically allocated: n_banks × MEMPAK_BANK_SIZE bytes */
	unsigned int   data_size;   /* total size in bytes (= n_banks × MEMPAK_BANK_SIZE)      */
	unsigned char  n_banks;     /* number of 32-KiB banks (1, 4, 16, or 62)               */
	unsigned char  file_format;

	char note_comments[MEMPAK_NUM_NOTES][MAX_NOTE_COMMENT_SIZE];
} mempak_structure_t;

/* Create a new, formatted mempak with the given number of banks (1–62).
 * Pass 0 or 1 for the standard single-bank Controller Pak. */
mempak_structure_t *mempak_new_ex(unsigned int n_banks);

/* Convenience wrapper — creates a standard 1-bank pak */
mempak_structure_t *mempak_new(void);

mempak_structure_t *mempak_loadFromFile(const char *filename);

int mempak_saveToFile(mempak_structure_t *mpk, const char *dst_filename, unsigned char format);
int mempak_exportNote(mempak_structure_t *mpk, int note_id, const char *dst_filename);
int mempak_importNote(mempak_structure_t *mpk, const char *notefile, int dst_note_id, int *note_id);
void mempak_free(mempak_structure_t *mpk);

int mempak_getFilenameFormat(const char *filename);
int mempak_string2format(const char *str);
const char *mempak_format2string(int fmt);
int mempak_hexdump(mempak_structure_t *pak);

#include "mempak_fs.h"

#endif // _mempak_h__

