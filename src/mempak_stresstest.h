#ifndef _mempak_stresstest_h__
#define _mempak_stresstest_h__

#include "raphnetadapter.h"

/**
 * @param pak_size  Total size in bytes to test. 0 = single-bank default (32 KiB).
 */
int mempak_stresstest(rnt_hdl_t hdl, int channel, int first_test, unsigned int pak_size);

#endif // _mempak_stresstest_h__
