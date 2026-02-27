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

	for (block = 0; block < total_size; block += 32)
	{
		unsigned int current_bank = block / MEMPAK_BANK_SIZE;
		unsigned int bank_offset  = block % MEMPAK_BANK_SIZE;

		u->cur_progress = block;
		u->update(u);

		/* Bank switch at 32-KiB boundaries */
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

	/* Reset to bank 0 */
	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	u->progressEnd(u, "Overwrite OK");
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

	/* Reset to bank 0 */
	if (n_banks > 1) {
		uint8_t bankbuf[32];
		memset(bankbuf, 0, sizeof(bankbuf));
		gcn64lib_mempak_writeBlock(hdl, channel, 0x8000, bankbuf);
	}

	u->progressEnd(u, "Verify OK");
	return 0;
}

int mempak_fill(rnt_hdl_t hdl, int channel, uint8_t pattern, int no_confirm, uiio *uio, unsigned int pak_size)
{
	uiio *u = getUIIO(uio);
	unsigned int total_size;
	unsigned int n_banks;
	int res;

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

	if (!no_confirm) {
		if (UIIO_YES != u->ask(UIIO_NOYES, "This test will erase your memory pak. Are you sure?")) {
			printf("Cancelled\n");
			return -1;
		}
	}

	///////////////////////////////////////////
	u->caption = "Fill with pattern";
	if (fill_pak(hdl, channel, u, pattern, total_size) < 0) {
		u->error("Error writing to mempak");
		return -1;
	}

	///////////////////////////////////////////
	u->multi_progress = 0; /* this is the last step */
	u->caption = "Verify fill";
	res = check_fill(hdl, channel, u, pattern, total_size);
	if (res < 0) {
		if (res == -1) {
			u->error("read error");
		}
		else if (res == -2) {
			u->error("verify failed");
		} else {
			u->error("unknown error");
		}
		return -1;
	}

	return 0;
}
