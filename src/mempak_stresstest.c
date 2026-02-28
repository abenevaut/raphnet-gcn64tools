/*	gcn64ctl : raphnet adapter management tools
	Copyright (C) 2007-2017  Raphael Assenat <raph@raphnet.net>

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
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include "mempak_stresstest.h"
#include "hexdump.h"
#include "mempak_gcn64usb.h"
#include "mempak.h"
#include "uiio.h"

#define LFSR_SEED	0xACE1
static uint16_t lfsr = LFSR_SEED;

static void lfsr_clock(void)
{
	unsigned lsb = lfsr & 1;   /* Get LSB (i.e., the output bit). */
	lfsr >>= 1;                /* Shift register */
	if (lsb)                   /* If the output bit is 1, apply toggle mask. */
	lfsr ^= 0xB400u;
}

static int fill_pak_pseudoRandom(rnt_hdl_t hdl, unsigned char channel, uiio *u, uint16_t seed, unsigned int total_size)
{
	unsigned int block, i;
	unsigned int n_banks = total_size / MEMPAK_BANK_SIZE;
	uint8_t fill[32];
	int res;

	lfsr = seed;

	u->cur_progress = 0;
	u->max_progress = total_size - 32;
	u->progressStart(u);

	/* Ensure bank 0 is selected before starting (pak resets to bank 0 on
	 * reconnect, but be explicit for multi-bank paks) */
	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	for (block = 0; block < total_size; block += 32)
	{
		unsigned int current_bank = block / MEMPAK_BANK_SIZE;
		unsigned int bank_offset  = block % MEMPAK_BANK_SIZE;

		u->cur_progress = block;
		u->update(u);

		if (n_banks > 1 && bank_offset == 0 && current_bank > 0) {
			uint8_t bankbuf[32];
			memset(bankbuf, (uint8_t)current_bank, sizeof(bankbuf));
			gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
		}

		for (i = 0; i < 32; i += 2) {
			fill[i]   = lfsr;
			fill[i+1] = lfsr >> 8;
			lfsr_clock();
		}

		res = gcn64lib_mempak_writeBlock(hdl, channel, (unsigned short)bank_offset, fill);
		if (res < 0) {
			return -1;
		}
	}

	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	u->progressEnd(u, "OK");
	return 0;
}

static int verify_pak_pseudoRandom(rnt_hdl_t hdl, unsigned char channel, uiio *u, uint16_t seed, unsigned int total_size)
{
	unsigned int block, i;
	unsigned int n_banks = total_size / MEMPAK_BANK_SIZE;
	uint8_t fill[32];
	uint8_t actual[32];
	int res;

	lfsr = seed;

	u->cur_progress = 0;
	u->max_progress = total_size - 32;
	u->progressStart(u);

	/* After a physical disconnect/reconnect (tests 7 & 8), the pak resets to
	 * bank 0. Force explicit bank 0 selection before iterating. */
	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	for (block = 0; block < total_size; block += 32)
	{
		unsigned int current_bank = block / MEMPAK_BANK_SIZE;
		unsigned int bank_offset  = block % MEMPAK_BANK_SIZE;

		u->cur_progress = block;
		u->update(u);

		if (n_banks > 1 && bank_offset == 0 && current_bank > 0) {
			uint8_t bankbuf[32];
			memset(bankbuf, (uint8_t)current_bank, sizeof(bankbuf));
			gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
		}

		for (i = 0; i < 32; i += 2) {
			fill[i]   = lfsr;
			fill[i+1] = lfsr >> 8;
			lfsr_clock();
		}

		res = gcn64lib_mempak_readBlock(hdl, channel, (unsigned short)bank_offset, actual);
		if (res < 0) {
			return -1;
		}
		if (memcmp(fill, actual, sizeof(actual))) {
			printf("\n");
			printf("Expected: "); printHexBuf(fill, 32);
			printf("Readback: "); printHexBuf(actual, 32);
			return -2;
		}
	}

	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	u->progressEnd(u, "OK");
	return 0;
}

static int fill_pak(rnt_hdl_t hdl, unsigned char channel, uiio *u, uint8_t v, unsigned int total_size)
{
	unsigned int block;
	unsigned int n_banks = total_size / MEMPAK_BANK_SIZE;
	uint8_t fill[32];
	int res;

	memset(fill, v, sizeof(fill));

	u->cur_progress = 0;
	u->max_progress = total_size - 32;
	u->progressStart(u);

	/* Ensure bank 0 is active before starting */
	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	for (block = 0; block < total_size; block += 32)
	{
		unsigned int current_bank = block / MEMPAK_BANK_SIZE;
		unsigned int bank_offset  = block % MEMPAK_BANK_SIZE;

		u->cur_progress = block;
		u->update(u);

		if (n_banks > 1 && bank_offset == 0 && current_bank > 0) {
			uint8_t bankbuf[32];
			memset(bankbuf, (uint8_t)current_bank, sizeof(bankbuf));
			gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
		}

		res = gcn64lib_mempak_writeBlock(hdl, channel, (unsigned short)bank_offset, fill);
		if (res < 0) {
			return -1;
		}
	}

	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	u->progressEnd(u, "OK");
	return 0;
}

static int check_fill(rnt_hdl_t hdl, unsigned char channel, uiio *u, uint8_t v, unsigned int total_size)
{
	unsigned int block;
	unsigned int n_banks = total_size / MEMPAK_BANK_SIZE;
	uint8_t expected[32];
	uint8_t buf[32];
	int res;

	memset(expected, v, sizeof(expected));

	u->cur_progress = 0;
	u->max_progress = total_size - 32;
	u->progressStart(u);

	/* Ensure bank 0 is active before starting */
	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	for (block = 0; block < total_size; block += 32)
	{
		unsigned int current_bank = block / MEMPAK_BANK_SIZE;
		unsigned int bank_offset  = block % MEMPAK_BANK_SIZE;

		u->cur_progress = block;
		u->update(u);

		if (n_banks > 1 && bank_offset == 0 && current_bank > 0) {
			uint8_t bankbuf[32];
			memset(bankbuf, (uint8_t)current_bank, sizeof(bankbuf));
			gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
		}

		res = gcn64lib_mempak_readBlock(hdl, channel, (unsigned short)bank_offset, buf);
		if (res < 0) {
			return -1;
		}
		if (memcmp(expected, buf, sizeof(expected))) {
			printf("\n");
			printf("Expected: "); printHexBuf(expected, 32);
			printf("Readback: "); printHexBuf(buf, 32);
			return -2;
		}
	}

	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	u->progressEnd(u, "OK");
	return 0;
}

static int burstTest(rnt_hdl_t hdl, unsigned char channel, uiio *u, int n_cycles, int n_blocks)
{
	uint8_t outbuf[32], inbuf[32];
	int cycle, i, res, block;

	lfsr = LFSR_SEED;

	u->cur_progress = 0;
	u->max_progress = n_cycles-1;
	u->progressStart(u);

	for (cycle=0; cycle<n_cycles; cycle++) {
		for (block = 0; block<n_blocks; block++)
		{
			u->cur_progress = cycle;
		 u->update(u);

			for (i=0; i<32; i+= 2) {
				outbuf[i]   = lfsr;
				outbuf[i+1] = lfsr >> 8;
				lfsr_clock();
			}

			res = gcn64lib_mempak_writeBlock(hdl, channel, block, outbuf);
			if (res < 0) {
				return -1;
			}

			res = gcn64lib_mempak_readBlock(hdl, channel, block, inbuf);
			if (res < 0) {
				return -1;
			}

			if (memcmp(inbuf, outbuf, sizeof(inbuf))) {
				printf("\n");
				printf("Expected: "); printHexBuf(outbuf, 32);
				printf("Readback: "); printHexBuf(inbuf, 32);
				return -2;
			}
		}
	}

	u->progressEnd(u, "OK");
	return 0;
}

int mempak_stresstest(rnt_hdl_t hdl, int channel, int first_test, unsigned int pak_size, int noconfirm)
{
	uiio *u = getUIIO(NULL);
	unsigned int total_size;
	unsigned int n_banks;
	int res;
	int no_disconnect = noconfirm; /* --noconfirm skips hot-disconnect/reconnect tests */

	/* Resolve size */
	if (pak_size == 0) {
		total_size = MEMPAK_MEM_SIZE;
	} else {
		n_banks = pak_size / MEMPAK_BANK_SIZE;
		if (n_banks == 0) n_banks = 1;
		if (n_banks > MEMPAK_BANKS_MAX) n_banks = MEMPAK_BANKS_MAX;
		total_size = n_banks * MEMPAK_BANK_SIZE;
	}

	u->multi_progress = 1;

	if (!noconfirm) {
		if (UIIO_YES != u->ask(UIIO_NOYES, "This test will erase your memory pak. Are you sure?")) {
			printf("Cancelled\n");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (first_test <= 1) {
		u->caption = "Test 1: Check for presence...\n";
		u->cur_progress = 0;
		u->max_progress = 1;
		u->progressStart(u);
		if (gcn64lib_mempak_detect(hdl, channel) < 0) {
			u->error("No mempak detected");
			return -1;
		}
		u->cur_progress = 1;
		u->update(u);
		u->progressEnd(u, "Mempak detected");
	}

	///////////////////////////////////////////
	if (first_test <= 2) {
		u->caption = "Test 2: Fill with 0x00";
		if (fill_pak(hdl, channel, u, 0, total_size) < 0) {
			u->error("Error writing to mempak");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (first_test <= 3) {
		u->caption = "Test 3: Verify fill";
		res = check_fill(hdl, channel, u, 0, total_size);
		if (res < 0) {
			if (res == -1) u->error("read error");
			else if (res == -2) u->error("verify failed");
			else u->error("unknown error");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (first_test <= 4) {
		u->caption = "Test 4: Write/Read burst on a single block";
		res = burstTest(hdl, channel, u, 100, 1);
		if (res < 0) {
			if (res == -1) u->error("IO error");
			else u->error("single block burst test failed");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (first_test <= 5) {
		u->caption = "Test 5: Write/Read burst on 10 blocks";
		res = burstTest(hdl, channel, u, 100, 10);
		if (res < 0) {
			if (res == -1) u->error("IO error");
			else u->error("10 block burst test failed");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (first_test <= 6) {
		u->caption = "Test 6: Fill with pseudo-random data";
		if (fill_pak_pseudoRandom(hdl, channel, u, LFSR_SEED, total_size) < 0) {
			u->error("Error writing to mempak");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (!no_disconnect)
	{
		if (first_test <= 7) {
			u->ask(UIIO_CONTINUE_ABORT, "Please disconnect the N64 controller or remove the memory pak");

			u->caption = "Test 7: Check for absence...\n";
			u->cur_progress = 0;
			u->max_progress = 1;
			u->progressStart(u);
			if (gcn64lib_mempak_detect(hdl, channel) < 0) {
				// Not found. Good.
			} else {
				u->progressEnd(u, "Mempak is still present");
				return -1;
			}

			u->cur_progress = 1;
			u->update(u);
			u->progressEnd(u, "Mempak not detected as expected");
		}

		///////////////////////////////////////////
		if (first_test <= 8) {
			u->ask(UIIO_CONTINUE_ABORT, "Please connect the N64 controller or re-insert the memory pak");

			u->caption = "Test 8: Check for presence again...\n";
			u->cur_progress = 0;
			u->max_progress = 1;
			u->progressStart(u);
			if (gcn64lib_mempak_detect(hdl, channel) < 0) {
				u->error("No mempak detected");
				return -1;
			}
			u->cur_progress = 1;
			u->update(u);
			u->progressEnd(u, "Mempak detected");
		}
	}

	///////////////////////////////////////////
	if (first_test <= 9) {
		u->caption = "Test 9: Verify pseudo-random values";
		res = verify_pak_pseudoRandom(hdl, channel, u, LFSR_SEED, total_size);
		if (res < 0) {
			if (res == -1) u->error("read error");
			else if (res == -2) u->error("verify failed");
			else u->error("unknown error");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (first_test <= 10) {
		u->caption = "Test 10: Fill with 0xFF";
		if (fill_pak(hdl, channel, u, 0xff, total_size) < 0) {
			u->error("Error writing to mempak");
			return -1;
		}
	}

	///////////////////////////////////////////
	if (first_test <= 11) {
		u->caption = "Test 11: Verify fill";
		res = check_fill(hdl, channel, u, 0xff, total_size);
		if (res < 0) {
			if (res == -1) u->error("read error");
			else if (res == -2) u->error("verify failed");
			else u->error("unknown error");
			return -1;
		}
	}

	return 0;
}
