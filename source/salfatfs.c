#include "fs_request.h"
#include "salio.h"
#include "fatfs/ff.h"
#include <wafel/utils.h>
#include <wafel/ios/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static const char* MODULE_NAME = "SALFATFS";

int (*FSFAT_init_stuff)(char*) = (void*)0x1078c834;
void (*FSFAT_set_mounted1)(bool) = (void*)0x1078a614;


static struct mounts {
    bool used;
    uint volume_handle;
    FATFS *fs;
    bool mounted;
    int mount_count;

} fatfs_mounts[FF_VOLUMES] = {};

typedef struct PathFIL {
    FIL fil;
    char path[512+4];
} PathFIL;


int salfatfs_find_index(uint volume_handle){
    for(int i=0; i<FF_VOLUMES; i++){
        if(fatfs_mounts[i].used && fatfs_mounts[i].volume_handle)
            return i;
    }
    return -1;
}

int salfatfs_add_volume(uint volume_handle, uint dev_handle){
    int index = salfatfs_find_index(volume_handle);
    if(index>=0){
        fatfs_mounts[index].mounted = false;
        salio_set_dev_handle(index, dev_handle);
        return index;
    }

    for(index=0; index<FF_VOLUMES; index++){
        if(!fatfs_mounts[index].used){
            fatfs_mounts[index].used = true;
            fatfs_mounts[index].volume_handle = volume_handle;
            fatfs_mounts[index].mounted = false;
            salio_set_dev_handle(index, dev_handle);
            return index;
        }
    }
    return -1;
}

FATFS* ff_allocate_FATFS(void){
    FATFS *fs = malloc_local(sizeof(FATFS));
    if(!fs)
        return fs;
    fs->win = iosAllocAligned(HEAPID_LOCAL, FF_MAX_SS, 0x20);
    if(!fs->win){
        free_local(fs);
        return NULL;
    }
    return fs;
}

void ff_free_FATFS(FATFS *fs){
    free_local(fs->win);
    free_local(fs);
}

PathFIL* ff_allocate_FIL(void){
    PathFIL *fp = malloc_local(sizeof(PathFIL));
    if(!fp)
        return fp;
    fp->fil.buf = iosAllocAligned(HEAPID_LOCAL, FF_MAX_SS, 0x20);
    if(!fp->fil.buf){
        free_local(fp);
        return NULL;
    }
    return fp;
}

void ff_free_FIL(PathFIL *fp){
    free_local(fp->fil.buf);
    free_local(fp);
}

static FSError fatfs_map_error(FRESULT error){
    switch (error)
    {
        case FR_OK: return FS_ERROR_OK;
        case FR_DISK_ERR: return FS_ERROR_MEDIA_ERROR;			/* (1) A hard error occurred in the low level disk I/O layer */
        case FR_INT_ERR: return FS_ERROR_OUT_OF_RESOURCES;				/* (2) Assertion failed */
        case FR_NOT_READY: return FS_ERROR_MEDIA_NOT_READY;			/* (3) The physical drive does not work */
        case FR_NO_FILE: return FS_ERROR_NOT_FOUND;				/* (4) Could not find the file */
        case FR_NO_PATH: return FS_ERROR_NOT_FOUND;				/* (5) Could not find the path */
        case FR_INVALID_NAME: return FS_ERROR_INVALID_PATH;		/* (6) The path name format is invalid */
        case FR_DENIED: return FS_ERROR_PERMISSION_ERROR;				/* (7) Access denied due to a prohibited access or directory full */
        case FR_EXIST: return FS_ERROR_PERMISSION_ERROR;				/* (8) Access denied due to a prohibited access */
        case FR_INVALID_OBJECT: return FS_ERROR_INVALID_FILEHANDLE;		/* (9) The file/directory object is invalid */
        case FR_WRITE_PROTECTED: return FS_ERROR_WRITE_PROTECTED;		/* (10) The physical drive is write protected */
        case FR_INVALID_DRIVE: return FS_ERROR_INVALID_MEDIA;		/* (11) The logical drive number is invalid */
        case FR_NOT_ENABLED: return FS_ERROR_NOT_INIT;			/* (12) The volume has no work area */
        case FR_NO_FILESYSTEM: return FS_ERROR_MEDIA_NOT_READY;		/* (13) Could not find a valid FAT volume */
        case FR_MKFS_ABORTED: return FS_ERROR_CANCELLED;		/* (14) The f_mkfs function aborted due to some problem */
        case FR_TIMEOUT: return FS_ERROR_BUSY;				/* (15) Could not take control of the volume within defined period */
        case FR_LOCKED: return FS_ERROR_BUSY;				/* (16) The operation is rejected according to the file sharing policy */
        case FR_NOT_ENOUGH_CORE: return FS_ERROR_OUT_OF_RESOURCES;		/* (17) LFN working buffer could not be allocated or given buffer is insufficient in size */
        case FR_TOO_MANY_OPEN_FILES: return FS_ERROR_MAX_FILES;	/* (18) Number of open files > FF_FS_LOCK */
        case FR_INVALID_PARAMETER: return FS_ERROR_INVALID_PARAM;	/* (19) Given parameter is invalid */    
        default: return -error;
    }
}

static BYTE parse_mode_str(char *mode_str){
    BYTE mode = 0;
    while(*mode_str){
        switch(*mode_str){
            case 'r':
                mode |= FA_READ;
                break;
            case 'v':
            case 'w':
                mode |= FA_CREATE_ALWAYS | FA_WRITE;
                break;
            case '+':
                mode |= FA_WRITE | FA_READ;
                break;
            case 'a':
                mode |= FA_OPEN_APPEND | FA_WRITE;
                break;
            case 'x':
                mode &=~FA_CREATE_ALWAYS;
                mode |= FA_CREATE_NEW;
                break;
            
        }
        mode_str++;
    }
    return mode;
}

static FSError fatfs_mount(int drive){
    if(fatfs_mounts[drive].mounted){
        fatfs_mounts[drive].mount_count++;
    }

    TCHAR path[5];
    snprintf(path, sizeof(path), "%d:", drive);
    
    debug_printf("%s: Mounting index: %d, path: %s, volume_handle: 0x%08x\n", MODULE_NAME, drive, path, fatfs_mounts[drive].volume_handle);

    if(!fatfs_mounts[drive].fs){
        fatfs_mounts[drive].fs = ff_allocate_FATFS();
        if(!fatfs_mounts[drive].fs)
            return FS_ERROR_OUT_OF_RESOURCES;
    }

    FRESULT res = f_mount(fatfs_mounts[drive].fs, path, 0);
    debug_printf("%s: mount returned %x\n", MODULE_NAME, res);
    if(res == FR_OK){
        fatfs_mounts[drive].mounted = true;
        fatfs_mounts[drive].mount_count = 1;;
    }
    return fatfs_map_error(res);
}

static FSError fatfs_open_file(FAT_OpenFileRequest *req, int drive){
    BYTE mode = parse_mode_str(req->mode);
    PathFIL *fp = ff_allocate_FIL();
    if(!fp) {
        return FS_ERROR_OUT_OF_RESOURCES;
    }
    snprintf(fp->path, sizeof(fp->path), "%d:%s", drive, req->path);

    FRESULT res = f_open(&fp->fil, fp->path, mode);
    debug_printf("%s: open_file %p, %s, 0x%x returned 0x%x\n", MODULE_NAME, fp, fp->path, mode, res);
    if(res != FR_OK){
        ff_free_FIL(fp);
        fp = NULL;
    }
    *req->filehandle_out_ptr = fp;
    return fatfs_map_error(res);
}

static FSError fatfs_read_file(FAT_ReadFileRequest *req){
    PathFIL *fp = *(req->file);
    debug_printf("%s: ReadFile(%p, %d, %d, %u, %p (%s), 0x%x)", MODULE_NAME, req->buffer, req->size, req->count, req->pos,fp, fp->path, req->flags);
    FRESULT res;
    if(req->flags & READ_REQUEST_WITH_POS){
        res = f_lseek(&fp->fil, req->pos);
        if(res != FR_OK)
            return fatfs_map_error(res);
    }
    UINT br;
    res = f_read(&fp->fil, req->buffer, req->size * req->count, &br);
    if(res != FR_OK)
        return fatfs_map_error(res);
    debug_printf("read: %d bytes\n", br);
    return br / req->size;
}

static FSError fatfs_stat_file(FAT_StatFileRequest *req, int drive) {
    PathFIL* fp = *req->fp;
    FILINFO info;
    debug_printf("%s: StatFile(%s)", MODULE_NAME, fp->path);
    FRESULT res = f_stat(fp->path, &info);
    if(res == FR_OK){
        FSStat *stat = req->stat;
        stat->flags = 0x0d000000;
        stat->mode = (info.fattrib & AM_RDO) ? 0x444:0x666;
        stat->size = info.fsize;

        FATFS *fs = fatfs_mounts[drive].fs;
        uint32_t cs = fs->csize * fs->ssize;
        uint32_t slack = cs - info.fsize % cs;
        stat->allocSize = info.fsize + slack;
        // stat->created = 0;
        // stat->modified = 0;
    }
    debug_printf("%s: StatFile(%s) -> size: %d res: %d\n", MODULE_NAME, fp->path, info.fsize, res);
    return fatfs_map_error(res);
}

static FSError fatfs_close_file(FAT_CloseFileRequest *req){
    PathFIL *fp = *req->file;
    debug_printf("%s: CloseFile(%s)\n", MODULE_NAME, fp->path);
    FSError res = f_close(&fp->fil);
    ff_free_FIL(fp);
    return fatfs_map_error(res);
}

static FSError fatfs_message_dispatch(FAT_WorkMessage *message){

    // commands not requiring drive
    switch (message->command)
    {
        // case 0xb:
        //     break;
        case 0x2a:
            char drive_char;
            message->worker->retval = FSFAT_init_stuff(&drive_char);
            message->worker->set = 1;
            return 0;
        case 0x2c: 
            FSFAT_set_mounted1(true);
            message->worker->retval = 1;
            message->worker->set = 1;
            //debug_printf("%s: Ignoring command %x\n", MODULE_NAME, message->command);
            return 0; 
    
    }

    int drive = salfatfs_find_index(message->volume_handle);
    if(drive<0){
        debug_printf("%s: Unknown volume: %08X\n", MODULE_NAME, message->volume_handle);
        return -1;
    }

    //commands requirering a valid drive
    switch(message->command){
        case 0x02:
            return fatfs_mount(drive);
        //case 0x03:
            //deattached
        case 0x0a:
            return fatfs_open_file(&message->request.open_file, drive);
        case 0x0b:
            return fatfs_read_file(&message->request.read_file);
        case 0x10:
            return fatfs_stat_file(&message->request.stat_file, drive);
        case 0x13:
            return fatfs_close_file(&message->request.close_file);
    }

    debug_printf("%s: Unknown command 0x%x!!!! HALTING\n", MODULE_NAME, message->command);
    while(1);
    return -1;
}

void salfatfs_process_message(FAT_WorkMessage *message){
    int ret = fatfs_message_dispatch(message);
    if(message->callback)
        message->callback(ret, message->calback_data);
}
