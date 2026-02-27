#ifndef _mempak_gcn64usb_h__
#define _mempak_gcn64usb_h__

#include "mempak.h"

uint16_t pak_address_crc( uint16_t address );
uint8_t pak_data_crc( const uint8_t *data, int n );

int gcn64lib_mempak_detect(rnt_hdl_t hdl, unsigned char channel);
int gcn64lib_mempak_readBlock(rnt_hdl_t hdl, unsigned char channel, unsigned short addr, unsigned char dst[32]);
int gcn64lib_mempak_writeBlock(rnt_hdl_t hdl, unsigned char channel, unsigned short addr, const unsigned char data[32]);

/**
 * Download a mempak from the hardware.
 *
 * @param pak_size  Total size in bytes to read. Pass 0 for the default
 *                  standard single-bank size (MEMPAK_MEM_SIZE = 32 KiB).
 *                  Multi-bank paks (e.g. Datel) can be read by specifying
 *                  MEMPAK_SIZE_128K, MEMPAK_SIZE_512K, etc.
 *                  The value is clamped to a valid multiple of MEMPAK_BANK_SIZE.
 */
int gcn64lib_mempak_download(rnt_hdl_t hdl, int channel, mempak_structure_t **mempak,
                             int (*progressCb)(int cur_addr, void *ctx), void *ctx,
                             unsigned int pak_size);

/**
 * Upload a mempak to the hardware.
 *
 * @param pak_size  Total size in bytes to write. Pass 0 to use the size
 *                  stored in the mempak_structure_t (pak->data_size).
 */
int gcn64lib_mempak_upload(rnt_hdl_t hdl, int channel, mempak_structure_t *pak,
                           int (*progressCb)(int cur_addr, void *ctx), void *ctx,
                           unsigned int pak_size);

#endif // _mempak_gcn64usb_h__
