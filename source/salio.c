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

uint32_t (*FSSAL_RawWrite)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount, const void *buf,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x0107328a0;

uint32_t (*FSSAL_Sync)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x10732314;

FSSALDevice* (*FSSAL_LookupDevice)(FSSALHandle device) = (void*)0x10733990;


static uint32_t device_handles[FF_VOLUMES];
static uint32_t sector_sizes[FF_VOLUMES];
static bool sync_unsupported[FF_VOLUMES];

void salio_set_dev_handle(int index, uint dev_handle){
    device_handles[index] = dev_handle;
    FSSALDevice* device = FSSAL_LookupDevice(dev_handle);
    sector_sizes[index] = device->block_size;
    sync_unsupported[index] = false;
}

DSTATUS disk_initialize (BYTE pdrv){
    return 0;
}

DSTATUS disk_status (BYTE pdrv) {
    return 0; // TODO
}

static BYTE aligned_buffer[512 * 128] ALIGNED(SALIO_ALIGNMENT);

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    //debug_printf("%s: disk_read(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, buff, (uint)sector, count);
    int res;
    if((uint)buff % SALIO_ALIGNMENT == 0){
        res = FSSAL_RawRead(device_handles[pdrv], sector>>32,sector, count, buff, NULL, NULL);
        return res?RES_ERROR:RES_OK;
    }

    debug_printf("%s: unaligned disk_read(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, buff, (uint)sector, count);

    u32 sector_size = sector_sizes[pdrv];
    int buffer_sectors = sizeof(aligned_buffer) / sector_size;

    void (*memcpy)(void*,const void*, u32) = memcpy32;
    if((uint)buff % 4) {
        memcpy = memcpy16;
        if((uint)buff % 2)
            memcpy = memcpy8;
    }

    while(count){
        UINT to_rw = min(count, buffer_sectors);
        res = FSSAL_RawRead(device_handles[pdrv], sector>>32,sector, to_rw, aligned_buffer, NULL, NULL);
        if(res)
            return RES_ERROR;
        memcpy(buff, aligned_buffer, to_rw * sector_size);
        buff += to_rw* sector_size;
        count-= to_rw;
    }
    return RES_OK;
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    int res;
    if((uint)buff % SALIO_ALIGNMENT == 0){
        res = FSSAL_RawWrite(device_handles[pdrv], sector>>32,sector, count, buff, NULL, NULL);
        debug_printf("%s: disk_write(%d, %p, %d, %d) -> 0x%x\n", MODULE_NAME, pdrv, buff, (uint)sector, count, res);
        return res?RES_ERROR:RES_OK;
    }

    debug_printf("%s: unaligned disk_write(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, buff, (uint)sector, count);

    u32 sector_size = sector_sizes[pdrv];
    UINT buffer_sectors = sizeof(aligned_buffer) / sector_size;

    void (*memcpy)(void*,const void*, u32) = memcpy32;
    if((uint)buff % 4) {
        memcpy = memcpy16;
        if((uint)buff % 2)
            memcpy = memcpy8;
    }

    while(count){
        UINT to_rw = min(count, buffer_sectors);
        memcpy(aligned_buffer, buff, to_rw * sector_size);
        res = FSSAL_RawWrite(device_handles[pdrv], sector>>32,sector, to_rw, aligned_buffer, NULL, NULL);
        if(res){
            debug_printf("%s: unaligned disk_write(%d, %p, %d, %d) -> failed 0x%x\n", MODULE_NAME, pdrv, buff, (uint)sector, count, res);
            return RES_ERROR;
        }
        buff += to_rw* sector_size;
        count-= to_rw;
    }
    return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff){
    debug_printf("%s: disk_ioctl(%i, %i, %p)\n", MODULE_NAME, pdrv, cmd, buff);

    switch (cmd)
    {
        case CTRL_SYNC:
            if(sync_unsupported[pdrv])
                return RES_OK;
            int res = FSSAL_Sync(device_handles[pdrv], 0, 0, 0, NULL, NULL);
            debug_printf("SYNC res: 0x%x\n", res);
            if(res == -0x90002){ // not supported
                sync_unsupported[pdrv] = true;
                return RES_OK;
            }
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
