/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
	Copyright (C) 2007-2015  Raphael Assenat <raph@raphnet.net>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define _GNU_SOURCE // for strcasestr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <libgen.h>
#include "mempak.h"

#define DEXDRIVE_DATA_OFFSET	0x1040
#define DEXDRIVE_COMMENT_OFFSET	0x40

mempak_structure_t *mempak_new_ex(unsigned int n_banks)
{
	mempak_structure_t *mpk;

	if (n_banks == 0) n_banks = 1;
	if (n_banks > MEMPAK_BANKS_MAX) n_banks = MEMPAK_BANKS_MAX;

	mpk = calloc(1, sizeof(mempak_structure_t));
	if (!mpk) {
		perror("calloc");
		return NULL;
	}

	mpk->n_banks   = (unsigned char)n_banks;
	mpk->data_size = n_banks * MEMPAK_BANK_SIZE;
	mpk->data      = calloc(1, mpk->data_size);
	if (!mpk->data) {
		perror("calloc data");
		free(mpk);
		return NULL;
	}

	format_mempak(mpk);

	return mpk;
}

mempak_structure_t *mempak_new(void)
{
	return mempak_new_ex(1);
}

static int mempak_findFreeNote(mempak_structure_t *mpk, entry_structure_t *entry_data, int *note_id)
{
	int i;

	if (!mpk)
		return -1;
	if (!entry_data)
		return -1;

	for (i=0; i<MEMPAK_NUM_NOTES; i++) {

		if (0 != get_mempak_entry(mpk, i, entry_data)) {
			return -1;
		}

		if (!entry_data->valid) {
			if (note_id) {
				*note_id = i;
			}
			return 0;
		}
	}

	return -1;
}

/**
 * \param mpk The memory pack to operate on
 * \param notefile The filename of the note to load
 * \param dst_note_id 0-15: (Over)write to specific note, -1: auto (first free)
 * \param note_id Stores the id of the note that was used
 * \return -1: Error, -2: Not enough space in mempak
 */
int mempak_importNote(mempak_structure_t *mpk, const char *notefile, int dst_note_id, int *note_id)
{
	int free_blocks = get_mempak_free_space(mpk);
	FILE *fptr;
	unsigned char entry_data[32];
	unsigned char *data = NULL;
	entry_structure_t entry;
	int res;

	if (dst_note_id < -1 || dst_note_id > 15) {
		fprintf(stderr, "Out of bound dst_note_id\n");
		return -1;
	}

	printf("Current free blocks: %d\n", free_blocks);

	fptr = fopen(notefile, "rb");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	if (1 != fread(entry_data, 32, 1, fptr)) {
		perror("fread");
		fclose(fptr);
		return -1;
	}

	/* I follow the same convention as bryc's javascript mempak editor[1]
	 * by looking for an inode number of 0xCAFE.
 	 *
	 * [1] https://github.com/bryc/mempak
	 *
	 * If there are other note formats to support, this will need updating.
	 */
	if ((entry_data[0x06] == 0xCA) &&
		(entry_data[0x07] == 0xFE))
	{
		entry_structure_t oldentry;
		long filesize;

		// Fixup the inode number (0xCAFE is invalid)
		entry_data[0x06] = 0x00;
		entry_data[0x07] = 0x05; // BLOCK_VALID_FIRST;

		if (0 != mempak_parse_entry(entry_data, &entry)) {
			fprintf(stderr, "Error loading note (invalid)\n");
			fclose(fptr);
			return -1;
		}

		fseek(fptr, 0, SEEK_END);
		filesize = ftell(fptr);
		fseek(fptr, 32, SEEK_SET);

		// Remove the note header
		filesize -= 32;
		if (filesize % MEMPAK_BLOCK_SIZE) {
			fprintf(stderr, "Invalid note file size\n");
			fclose(fptr);
			return -1;
		}

		entry.blocks = filesize / MEMPAK_BLOCK_SIZE;

		printf("Note size: %d blocks\n", entry.blocks);
		printf("Note name: %s\n", entry.utf8_name);

		if (entry.blocks > free_blocks) {
			fprintf(stderr, "Not enough space (note is %d blocks and only %d free blocks in mempak)\n",
				entry.blocks, free_blocks);
			fclose(fptr);
			return -2;
		}

		data = calloc(1, entry.blocks * MEMPAK_BLOCK_SIZE);
		if (!data) {
			perror("calloc");
			fclose(fptr);
			return -1;
		}

		if (1 != fread(data, entry.blocks * MEMPAK_BLOCK_SIZE, 1, fptr)) {
			perror("could not load note data");
			free(data);
			fclose(fptr);
			return -1;
		}

		if (dst_note_id == -1) { // Auto (first free note)
			if (0 != mempak_findFreeNote(mpk, &oldentry, note_id)) {
				fprintf(stderr, "Could not find an empty note\n");
				free(data);
				fclose(fptr);
				return -1;
			}
		} else { // Specific note
			get_mempak_entry(mpk, dst_note_id, &oldentry);
			if (oldentry.valid) {
				printf("Overwriting note %d\n", dst_note_id);
				delete_mempak_entry(mpk, &oldentry);
			} else {
				fprintf(stderr, "No note id %d\n", dst_note_id);
				free(data);
				fclose(fptr);
				return -1;
			}
			if (note_id)
				*note_id = dst_note_id;
		}

		res = write_mempak_entry_data(mpk, &entry, data);
		if (res != 0) {
			fprintf(stderr, "Failed to write note (error %d)\n", res);
			free(data);
			fclose(fptr);
			return -1;
		}

		return 0;
	} else {
		fprintf(stderr, "Input file does not appear to be in a supported format.\n");
	}

	if (data)
		free(data);
	fclose(fptr);
	return -1;
}

int mempak_exportNote(mempak_structure_t *mpk, int note_id, const char *dst_filename)
{
	FILE *fptr;
	entry_structure_t note_header;
	unsigned char databuf[0x10000];

	if (!mpk)
		return -1;

	if (0 != get_mempak_entry(mpk, note_id, &note_header)) {
		fprintf(stderr, "Error accessing note\n");
		return -1;
	}

	if (!note_header.valid) {
		fprintf(stderr, "Invalid note\n");
		return -1;
	}

	if (0 != read_mempak_entry_data(mpk, &note_header, databuf)) {
		fprintf(stderr, "Error accessing note data\n");
		return -1;
	}

	fptr = fopen(dst_filename, "wb");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	/* For compatibility with bryc's javascript mempak editor[1], I set
	 * the inode number to 0xCAFE.
	 *
	 * [1] https://github.com/bryc/mempak
	 */
	note_header.raw_data[0x06] = 0xCA;
	note_header.raw_data[0x07] = 0xFE;

	fwrite(note_header.raw_data, 32, 1, fptr);
	fwrite(databuf, MEMPAK_BLOCK_SIZE, note_header.blocks, fptr);
	fclose(fptr);

	return 0;
}

int mempak_saveToFile(mempak_structure_t *mpk, const char *dst_filename, unsigned char format)
{
	FILE *fptr;
	int i;

	if (!mpk)
		return -1;

	fptr = fopen(dst_filename, "wb");
	if (!fptr) {
		perror("fopen");
		return -1;
	}

	switch(format)
	{
		default:
			fclose(fptr);
			return -1;

		case MPK_FORMAT_MPK:
			/* Write all banks as-is (multi-bank files are stored contiguously) */
			fwrite(mpk->data, mpk->data_size, 1, fptr);
			break;

		case MPK_FORMAT_MPK4:
			/* Legacy 4-image format: repeat bank 0 four times for 1-bank paks,
			 * or write all banks contiguously for multi-bank paks. */
			if (mpk->n_banks == 1) {
				fwrite(mpk->data, mpk->data_size, 1, fptr);
				fwrite(mpk->data, mpk->data_size, 1, fptr);
				fwrite(mpk->data, mpk->data_size, 1, fptr);
				fwrite(mpk->data, mpk->data_size, 1, fptr);
			} else {
				fwrite(mpk->data, mpk->data_size, 1, fptr);
			}
			break;

		case MPK_FORMAT_N64:
			/* DexDrive format: "123-456-STD" header, comments at 0x40,
			 * data at 0x1040. Supports multi-bank by writing all banks
			 * contiguously after the header. */
			fprintf(fptr, "123-456-STD");

			fseek(fptr, DEXDRIVE_COMMENT_OFFSET, SEEK_SET);
			for (i=0; i<MEMPAK_NUM_NOTES; i++) {
				unsigned char tmp = 0;
				fwrite(mpk->note_comments[i], 255, 1, fptr);
				fwrite(&tmp, 1, 1, fptr);
			}

			fseek(fptr, DEXDRIVE_DATA_OFFSET, SEEK_SET);
			fwrite(mpk->data, mpk->data_size, 1, fptr);
			break;
	}

	fclose(fptr);
	return 0;
}

mempak_structure_t *mempak_loadFromFile(const char *filename)
{
	FILE *fptr;
	long file_size;
	int i;
	long offset = 0;
	unsigned int n_banks = 0;
	mempak_structure_t *mpk;

	mpk = calloc(1, sizeof(mempak_structure_t));
	if (!mpk) {
		perror("calloc");
		return NULL;
	}

	fptr = fopen(filename, "rb");
	if (!fptr) {
		perror("fopen");
		free(mpk);
		return NULL;
	}

	fseek(fptr, 0, SEEK_END);
	file_size = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);

	printf("File size: %ld bytes\n", file_size);

	/* Raw binary images.
	 * Accept any multiple of MEMPAK_BANK_SIZE (1, 4, 16, 62 banks, etc.)
	 * The old 4-image MPK4 format corresponds to 4 banks. */
	if (file_size > 0 && (file_size % MEMPAK_BANK_SIZE) == 0) {
		unsigned int banks = (unsigned int)(file_size / MEMPAK_BANK_SIZE);
		if (banks >= 1 && banks <= MEMPAK_BANKS_MAX) {
			n_banks = banks;
			printf("MPK binary: %u bank(s)\n", n_banks);
			if (banks == 1) {
				mpk->file_format = MPK_FORMAT_MPK;
			} else {
				mpk->file_format = MPK_FORMAT_MPK4;
			}
		}
	}

	if (n_banks == 0) {
		char header[11];
		char *magic = "123-456-STD";
		/* If the size is not a recognised multiple, it could be a .N64 file */
		fread(header, 11, 1, fptr);
		if (0 == memcmp(header, magic, sizeof(header))) {
			printf(".N64 file detected\n");

			fseek(fptr, DEXDRIVE_COMMENT_OFFSET, SEEK_SET);
#if MAX_NOTE_COMMENT_SIZE != 257
#error "MAX_NOTE_COMMENT_SIZE must be 257"
#endif
			for (i=0; i<16; i++) {
				fread(mpk->note_comments[i], 256, 1, fptr);
				mpk->note_comments[i][256] = 0;
			}

			offset = DEXDRIVE_DATA_OFFSET;
			mpk->file_format = MPK_FORMAT_N64;

			/* Determine bank count from remaining data after header */
			long data_bytes = file_size - DEXDRIVE_DATA_OFFSET;
			if (data_bytes > 0 && (data_bytes % MEMPAK_BANK_SIZE) == 0) {
				n_banks = (unsigned int)(data_bytes / MEMPAK_BANK_SIZE);
				if (n_banks > MEMPAK_BANKS_MAX) n_banks = MEMPAK_BANKS_MAX;
			} else {
				/* Fallback: 1 bank */
				n_banks = 1;
			}
		}
	}

	if (n_banks == 0) {
		fprintf(stderr, "Unrecognised mempak file format or size\n");
		fclose(fptr);
		free(mpk);
		return NULL;
	}

	mpk->n_banks   = (unsigned char)n_banks;
	mpk->data_size = n_banks * MEMPAK_BANK_SIZE;
	mpk->data      = calloc(1, mpk->data_size);
	if (!mpk->data) {
		perror("calloc data");
		fclose(fptr);
		free(mpk);
		return NULL;
	}

	fseek(fptr, offset, SEEK_SET);
	fread(mpk->data, mpk->data_size, 1, fptr);
	fclose(fptr);

	return mpk;
}

void mempak_free(mempak_structure_t *mpk)
{
	if (mpk) {
		free(mpk->data);
		free(mpk);
	}
}

const char *mempak_format2string(int fmt)
{
	switch(fmt)
	{
		case MPK_FORMAT_MPK: return "MPK";
		case MPK_FORMAT_MPK4: return "MPK4";
		case MPK_FORMAT_N64: return "N64";
		case MPK_FORMAT_INVALID: return "Invalid";
		default:
			return "Unknown";
	}
}

int mempak_string2format(const char *str)
{
	if (0 == strcasecmp(str, "mpk"))
		return MPK_FORMAT_MPK;

	if (0 == strcasecmp(str, "mpk4"))
		return MPK_FORMAT_MPK4;

	if (0 == strcasecmp(str, "n64"))
		return MPK_FORMAT_N64;

	return MPK_FORMAT_INVALID;
}

int mempak_getFilenameFormat(const char *filename)
{
	char *s;

	if ((s = strcasestr(filename, ".N64"))) {
		if (s[4] == 0)
			return MPK_FORMAT_N64;
	}
	if ((s = strcasestr(filename, ".MPK"))) {
		if (s[4] == 0)
			return MPK_FORMAT_MPK4;
	}

	return MPK_FORMAT_INVALID;
}

int mempak_hexdump(mempak_structure_t *pak)
{
	unsigned int i;
	int j;

	if (!pak || !pak->data) return -1;

	for (i=0; i < pak->data_size; i+=0x20) {
		printf("%05x: ", i);
		for (j=0; j<0x20; j++) {
			printf("%02x ", pak->data[i+j]);
		}
		printf("    ");

		for (j=0; j<0x20; j++) {
			printf("%c", isprint(pak->data[i+j]) ? pak->data[i+j] : '.' );
		}
		printf("\n");
	}

	return 0;
}

