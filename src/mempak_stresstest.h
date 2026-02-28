#ifndef _mempak_stresstest_h__
#define _mempak_stresstest_h__

#include "raphnetadapter.h"

/**
 * @param pak_size      Total size in bytes to test. 0 = single-bank default (32 KiB).
 * @param noconfirm     If non-zero, skip the initial confirmation prompt AND the
 *                      hot-disconnect/reconnect tests (tests 7 & 8).
 */
int mempak_stresstest(rnt_hdl_t hdl, int channel, int first_test, unsigned int pak_size, int noconfirm);

#endif // _mempak_stresstest_h__
