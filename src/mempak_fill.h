#ifndef _mempak_fill_h__
#define _mempak_fill_h__

#include <stdint.h>
#include "raphnetadapter.h"
#include "uiio.h"

/**
 * Fill (and verify) a mempak with a repeating byte pattern.
 * @param pak_size  Total size in bytes to fill. 0 = single-bank default (32 KiB).
 */
int mempak_fill(rnt_hdl_t hdl, int channel, uint8_t pattern, int no_confirm, uiio *uio, unsigned int pak_size);

#endif // _mempak_fill_h__
