#include "fs_request.h"
#include "salio.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "fatfs/ffconf.h"
#include <stdbool.h>
#include <stdio.h>
#include <wafel/dynamic.h>
#include <wafel/utils.h>
#include <wafel/ios/svc.h>

static const char *MODULE_NAME = "SALIO";



uint32_t (*FSSAL_RawRead)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount, void *buf,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x10732bc0;

uint32_t (*FSSAL_RawWrite)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount, void *buf,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x0107328a0;

uint32_t (*FSSAL_Sync)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x10732314;

FSSALDevice* (*FSSAL_LookupDevice)(FSSALHandle device) = (void*)0x10733990;


static uint32_t device_handles[FF_VOLUMES];
static uint32_t sector_sizes[FF_VOLUMES];

void salio_set_dev_handle(int index, uint dev_handle){
    device_handles[index] = dev_handle;
    FSSALDevice* device = FSSAL_LookupDevice(dev_handle);
    sector_sizes[index] = device->block_size;
}

DSTATUS disk_initialize (BYTE pdrv){
    return 0;
}

DSTATUS disk_status (BYTE pdrv) {
    return 0; // TODO
}

struct rw_cb_ctx {
    int semaphore;
    int res;
} typedef rw_cb_ctx;

static void rw_callback(int res, void *ctx){
    debug_printf("%s: rw_callback res: %x\n", MODULE_NAME, res);
    rw_cb_ctx *c = (rw_cb_ctx *)ctx;
    c->res = res;
    iosSignalSemaphore(c->semaphore);
}

static DRESULT aligned_sync_rw(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count, uint32_t (*raw_rw)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,void *buf,
                      void (*cb)(int, void*),void *cb_ctx)){

    int res = raw_rw(device_handles[pdrv], sector>>32, sector, count, buff, NULL, NULL);
    debug_printf("%s: raw_rw returned: %x\n", MODULE_NAME, res);
    return res?RES_ERROR:RES_OK;
}

static DRESULT sync_rw(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count, uint32_t (*raw_rw)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,void *buf,
                      void (*cb)(int, void*),void *cb_ctx)){
    if((uint)buff % SALIO_ALIGNMENT == 0){
        return aligned_sync_rw(pdrv, buff, sector, count, raw_rw);
    }

    u32 sector_size = sector_sizes[pdrv];
    static u8 aligned_buffer[512 * 128] ALIGNED(SALIO_ALIGNMENT);
    int buffer_sectors = sizeof(aligned_buffer) / sector_size;

    void (*memcpy)(void*,void*, u32) = memcpy32;
    if((uint)buff % 4) {
        memcpy = memcpy16;
        if((uint)buff % 2)
            memcpy = memcpy8;
    }

    while(count){
        UINT to_rw = min(count, buffer_sectors);
        int res = aligned_sync_rw(pdrv, aligned_buffer, sector, to_rw, raw_rw);
        if(res)
            return res;
        memcpy(buff, aligned_buffer, to_rw * sector_size);
        buff += to_rw* sector_size;
        count-= to_rw;
    }
    return RES_OK;
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    debug_printf("%s: disk_read(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, buff, (uint)sector, count);
    return sync_rw(pdrv, buff, sector, count, FSSAL_RawRead);
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    return sync_rw(pdrv, (BYTE*)buff, sector, count, FSSAL_RawWrite);
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff){
    debug_printf("%s: disk_ioctl(%i, %i, %p)\n", MODULE_NAME, pdrv, cmd, buff);

    switch (cmd)
    {
        case CTRL_SYNC:
            int res = FSSAL_Sync(device_handles[pdrv], 0, 0, 0, NULL, NULL);
            debug_printf("SYNC res: 0x%x\n", res);
            return res?RES_ERROR:RES_OK;
        case GET_SECTOR_SIZE:
            debug_printf("SECTOR SIZE: %i\n", sector_sizes[pdrv]);
            *(WORD*)buff = sector_sizes[pdrv];
            return RES_OK;
        case GET_SECTOR_COUNT:
            FSSALDevice* device = FSSAL_LookupDevice(device_handles[pdrv]);
            if(!device)
                return RES_PARERR;
            LBA_t bc = device->block_count_hi;
            bc <<= 32;
            bc |= device->block_count;
            *(LBA_t*)buff = bc;
            return RES_OK;
    
    }
    debug_printf("UNKOWN IOCTL %i\n", cmd);
    return RES_PARERR;
}

DWORD get_fattime (void) {
    return 0; //TODO
}
