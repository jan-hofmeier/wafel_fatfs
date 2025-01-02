#include "fs_request.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "fatfs/ffconf.h"
#include <stdbool.h>
#include <stdio.h>
#include <wafel/dynamic.h>
#include <wafel/utils.h>
#include <wafel/ios/svc.h>

static const char *MODULE_NAME = "SALIO";



uint32_t (*FSSAL_RawRead)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,void *buf,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x10732bc0;

uint32_t (*FSSAL_RawWrite)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,void *buf,
                      void (*cb)(int, void*),void *cb_ctx) = (void*)0x0107328a0;

struct sal_fatfs {
    FSSALHandle handle;
    FATFS fs;
    bool used;
} typedef sal_fatfs;

static sal_fatfs fatfs_volumes[FF_VOLUMES] = {};


int salio_add_sal_handle(FSSALHandle sal_handle) {
    for(int i=0; i<FF_VOLUMES; i++){
        if(!fatfs_volumes[i].used){
            fatfs_volumes[i].handle = sal_handle;
            fatfs_volumes[i].used = true;
            return i;
        }
    }
    return -1;
}

int salio_get_drive_number(FSSALHandle handle){
    for(int i=0; i<FF_VOLUMES; i++){
        if(fatfs_volumes[i].used && fatfs_volumes[i].handle == handle)
            return i;
    }
    return -1;
}

void salio_remove_sal_volume(int pdrv){
    fatfs_volumes[pdrv].used = false;
}

FATFS* salio_get_fatfs(int pdrv){
    return &fatfs_volumes[pdrv].fs;
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

static DRESULT sync_rw(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count, uint32_t (*raw_rw)(FSSALHandle device,uint32_t lba_hi,uint lba, uint32_t blkCount,void *buf,
                      void (*cb)(int, void*),void *cb_ctx)){
    rw_cb_ctx ctx = {iosCreateSemaphore(1,0)};
    if(ctx.semaphore < 0){
        debug_printf("%s: Error creating Semaphore: 0x%X\n", MODULE_NAME, ctx.semaphore);
        return RES_NOTRDY;
    }
    int res = raw_rw(/* fatfs_volumes[pdrv].handle */ *((int*)0x10bb7e4c), sector>>32, sector, count, buff, rw_callback, &ctx);
    debug_printf("%s: raw_rw returned: %x\n", MODULE_NAME, res);
    if(!res){
        iosWaitSemaphore(ctx.semaphore, 0);
        res = ctx.res;
    }
    iosDestroySemaphore(ctx.semaphore);
    debug_printf("%s: sync_rw res: %x\n", MODULE_NAME, res);
    return res?RES_ERROR:RES_OK;
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    debug_printf("%s: disk_read(%d, %p, %ld, %d)\n", MODULE_NAME, pdrv, buff, sector, count);
    return sync_rw(pdrv, buff, sector, count, FSSAL_RawRead);
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    return sync_rw(pdrv, (BYTE*)buff, sector, count, FSSAL_RawWrite);
}
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff){
    debug_printf("%s: disk_ioctl(%i, %i, %p)\n", pdrv, cmd, buff);
    debug_printf("UNKOWN IOCTL %i: HALTING\n", cmd);
    while(1);
    return RES_OK;
}

DWORD get_fattime (void) {
    return 0; //TODO
}
