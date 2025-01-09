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
#include <wafel/ios/memory.h>

static const char *MODULE_NAME = "SALIO";

//#define FATFSIO_DEBUG 4

#ifdef FATFSIO_DEBUG
#define DPRINTF(n,s)    do { if ((n) <= FATFSIO_DEBUG) debug_printf s; } while (0)
#else
#define DPRINTF(n,s)    do {} while(0)
#endif

uint32_t (*FSSAL_RawRead)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount, void *buf,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x10732bc0;

uint32_t (*FSSAL_RawWrite)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount, const void *buf,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x0107328a0;

uint32_t (*FSSAL_Sync)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x10732314;

void (*FAT_GetDateTime)(uint16_t* date, uint16_t* time, void* something) = (void*) 0x10798d24;

FSSALDevice* (*FSSAL_LookupDevice)(FSSALHandle device) = (void*)0x10733990;


typedef struct salio_device {
    uint32_t device_handle;
    uint32_t sector_size;
    bool sync_unsupported;
    int semaphore;
    bool active;   
} salio_device;

static salio_device devices[FF_VOLUMES] = { };

int salio_set_dev_handle(int index, uint dev_handle){
    salio_device *dev = devices + index;
    dev->device_handle = dev_handle;
    FSSALDevice* sal_device = FSSAL_LookupDevice(dev_handle);
    dev->sector_size = sal_device->block_size;
    dev->sync_unsupported = false;
    if(dev->active)
        iosDestroySemaphore(dev->semaphore);
    int sem = iosCreateSemaphore(1,1);
    if(sem < 0) {
        return -1;
    }
    dev->semaphore = sem;
    dev->active = true;
    return 0;
}

DSTATUS disk_initialize (BYTE pdrv){
    return 0;
}

DSTATUS disk_status (BYTE pdrv) {
    return 0; // TODO
}

static BYTE aligned_buffer[512 * 128] ALIGNED(SALIO_ALIGNMENT);

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    salio_device *dev = devices + pdrv;
    DPRINTF(4, ("%s: disk_read(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, buff, (uint)sector, count));
    iosWaitSemaphore(dev->semaphore, 0);
    DPRINTF(4, ("%s: disk_read got semaphore\n", MODULE_NAME));
    iosSignalSemaphore(dev->semaphore);
    DPRINTF(4, ("%s: disk_read released semaphore\n", MODULE_NAME));
    int res;
    if((uint)buff % SALIO_ALIGNMENT == 0){
        res = FSSAL_RawRead(dev->device_handle, sector>>32,sector, count, buff, NULL, NULL);
        DPRINTF(3, ("%s: disk_read(%d, %p, %d, %d) -> 0x%x\n", MODULE_NAME, pdrv, buff, (uint)sector, count, res));
        return res?RES_ERROR:RES_OK;
    }

    DPRINTF(3, ("%s: unaligned disk_read(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, buff, (uint)sector, count));

    u32 sector_size = dev->sector_size;
    int buffer_sectors = sizeof(aligned_buffer) / sector_size;

    void (*memcpy)(void*,const void*, u32) = memcpy32;
    if((uint)buff % 4) {
        memcpy = memcpy16;
        if((uint)buff % 2)
            memcpy = memcpy8;
    }

    while(count){
        UINT to_rw = min(count, buffer_sectors);
        res = FSSAL_RawRead(dev->device_handle, sector>>32,sector, to_rw, aligned_buffer, NULL, NULL);
        if(res) {
            DPRINTF(3, ("%s: unaligned disk_read(%d, %p, %d, %d) -> failed 0x%x\n", MODULE_NAME, pdrv, buff, (uint)sector, count, res));
            return RES_ERROR;
        }
        memcpy(buff, aligned_buffer, to_rw * sector_size);
        buff += to_rw* sector_size;
        sector+=to_rw;
        count-= to_rw;
    }
    return RES_OK;
}

typedef struct write_cb_ctx {
    salio_device *dev;
    void *buff;
} write_cb_ctx;

static void write_callback(int res, void *ctx){
    write_cb_ctx *c = ctx;
    DPRINTF(4, ("%s: rw_callback res: %x buff: %p\n", MODULE_NAME, res, c->buff));
    iosSignalSemaphore(c->dev->semaphore);
    free_local(c->buff);
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    salio_device *dev = devices + pdrv;
    size_t bsz = count * dev->sector_size;
    void *copy = iosAllocAligned(HEAPID_LOCAL, bsz + sizeof(write_cb_ctx), SALIO_ALIGNMENT);
    int res;
    if(copy) {
        FS_memcpy(copy, buff, bsz);
        write_cb_ctx *ctx = copy + bsz;
        ctx->dev = dev;
        ctx->buff = copy; 
        iosWaitSemaphore(dev->semaphore, 0);
        DPRINTF(4, ("%s: disk_write(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, copy, (uint)sector, count));
        int res = FSSAL_RawWrite(dev->device_handle, sector>>32,sector, count, copy, write_callback, ctx);
        DPRINTF(3, ("%s: async disk_write(%d, %p, %d, %d) -> 0x%x\n", MODULE_NAME, pdrv, copy, (uint)sector, count, res));
        return res;
    }

    if((uint)buff % SALIO_ALIGNMENT == 0){
        res = FSSAL_RawWrite(dev->device_handle, sector>>32,sector, count, buff, NULL, NULL);
        DPRINTF(3, ("%s: disk_write(%d, %p, %d, %d) -> 0x%x\n", MODULE_NAME, pdrv, buff, (uint)sector, count, res));
        return res?RES_ERROR:RES_OK;
    }

    DPRINTF(3, ("%s: unaligned disk_write(%d, %p, %d, %d)\n", MODULE_NAME, pdrv, buff, (uint)sector, count));

    u32 sector_size = dev->sector_size;
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
        res = FSSAL_RawWrite(dev->device_handle, sector>>32,sector, to_rw, aligned_buffer, NULL, NULL);
        if(res){
            DPRINTF(3, ("%s: unaligned disk_write(%d, %p, %d, %d) -> failed 0x%x\n", MODULE_NAME, pdrv, buff, (uint)sector, count, res));
            return RES_ERROR;
        }
        buff += to_rw* sector_size;
        sector+=to_rw;
        count-= to_rw;
    }
    return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff){
    DPRINTF(3, ("%s: disk_ioctl(%i, %i, %p)\n", MODULE_NAME, pdrv, cmd, buff));

    salio_device *dev = devices + pdrv;
    switch (cmd)
    {
        case CTRL_SYNC:
            if(dev->sync_unsupported)
                return RES_OK;
            int res = FSSAL_Sync(dev->device_handle, 0, 0, 0, NULL, NULL);
            DPRINTF(3, ("SYNC res: 0x%x\n", res));
            if(res == -0x90002){ // not supported
                dev->sync_unsupported = true;
                return RES_OK;
            }
            return res?RES_ERROR:RES_OK;
        case GET_SECTOR_SIZE:
            DPRINTF(3, ("SECTOR SIZE: %i\n", dev->sector_size));
            *(WORD*)buff = dev->sector_size;
            //*(WORD*)buff = 4096;
            return RES_OK;
        case GET_SECTOR_COUNT:
            FSSALDevice* device = FSSAL_LookupDevice(dev->device_handle);
            if(!device)
                return RES_PARERR;
            LBA_t bc = device->block_count_hi;
            bc <<= 32;
            bc |= device->block_count;
            *(LBA_t*)buff = bc;
            return RES_OK;
    
    }
    DPRINTF(3, ("UNKOWN IOCTL %i\n", cmd));
    return RES_PARERR;
}

DWORD get_fattime (void) {
    uint16_t date, time;
    FAT_GetDateTime(&date, &time, NULL);
    DPRINTF(3, ("%s: get_fattime date: 0x%04X, time: 0x%04X \n", MODULE_NAME, date, time));
    return (DWORD)date<<16 | time;
}
