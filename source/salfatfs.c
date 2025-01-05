#include "fs_request.h"
#include "salio.h"
#include "fatfs/ff.h"
#include <wafel/utils.h>
#include <wafel/ios/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

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

typedef struct PathDIR {
    DIR dir;
    char path[512+4];
} PathDIR;

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
    fs->win = iosAllocAligned(HEAPID_LOCAL, FF_MAX_SS, SALIO_ALIGNMENT);
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
    fp->fil.buf = iosAllocAligned(HEAPID_LOCAL, FF_MAX_SS, SALIO_ALIGNMENT);
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

PathDIR* ff_allocate_DIR(void){
    return malloc_local(sizeof(PathDIR));
}

void ff_free_DIR(PathDIR *dp){
    free_local(dp);
}

static FATError fatfs_map_error(FRESULT error){
    switch (error)
    {
        case FR_OK: return FAT_ERROR_OK;
        case FR_DISK_ERR: return FAT_ERROR_MEDIA_ERROR;			/* (1) A hard error occurred in the low level disk I/O layer */
        case FR_INT_ERR: return FAT_ERROR_OUT_OF_RESOURCES;				/* (2) Assertion failed */
        case FR_NOT_READY: return FAT_ERROR_MEDIA_NOT_READY;			/* (3) The physical drive does not work */
        case FR_NO_FILE: return FAT_ERROR_NOT_FOUND;				/* (4) Could not find the file */
        case FR_NO_PATH: return FAT_ERROR_NOT_FOUND;				/* (5) Could not find the path */
        case FR_INVALID_NAME: return FAT_ERROR_INVALID_PATH;		/* (6) The path name format is invalid */
        case FR_DENIED: return FAT_ERROR_PERMISSION_ERROR;				/* (7) Access denied due to a prohibited access or directory full */
        case FR_EXIST: return FAT_ERROR_PERMISSION_ERROR;				/* (8) Access denied due to a prohibited access */
        case FR_INVALID_OBJECT: return FAT_ERROR_INVALID_FILEHANDLE;		/* (9) The file/directory object is invalid */
        case FR_WRITE_PROTECTED: return FAT_ERROR_WRITE_PROTECTED;		/* (10) The physical drive is write protected */
        case FR_INVALID_DRIVE: return FAT_ERROR_INVALID_MEDIA;		/* (11) The logical drive number is invalid */
        case FR_NOT_ENABLED: return FAT_ERROR_NOT_INIT;			/* (12) The volume has no work area */
        case FR_NO_FILESYSTEM: return FAT_ERROR_MEDIA_NOT_READY;		/* (13) Could not find a valid FAT volume */
        case FR_MKFS_ABORTED: return FAT_ERROR_CANCELLED;		/* (14) The f_mkfs function aborted due to some problem */
        case FR_TIMEOUT: return FAT_ERROR_BUSY;				/* (15) Could not take control of the volume within defined period */
        case FR_LOCKED: return FAT_ERROR_BUSY;				/* (16) The operation is rejected according to the file sharing policy */
        case FR_NOT_ENOUGH_CORE: return FAT_ERROR_OUT_OF_RESOURCES;		/* (17) LFN working buffer could not be allocated or given buffer is insufficient in size */
        case FR_TOO_MANY_OPEN_FILES: return FAT_ERROR_MAX_FILES;	/* (18) Number of open files > FF_FS_LOCK */
        case FR_INVALID_PARAMETER: return FAT_ERROR_INVALID_PARAM;	/* (19) Given parameter is invalid */    
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

static FATError fatfs_mount(int drive){
    if(fatfs_mounts[drive].mounted){
        fatfs_mounts[drive].mount_count++;
        return FAT_ERROR_OK;
    }

    TCHAR path[5];
    snprintf(path, sizeof(path), "%d:", drive);
    
    debug_printf("%s: Mounting index: %d, path: %s, volume_handle: 0x%08x\n", MODULE_NAME, drive, path, fatfs_mounts[drive].volume_handle);

    if(!fatfs_mounts[drive].fs){
        fatfs_mounts[drive].fs = ff_allocate_FATFS();
        if(!fatfs_mounts[drive].fs)
            return FAT_ERROR_OUT_OF_RESOURCES;
    }

    FRESULT res = f_mount(fatfs_mounts[drive].fs, path, 0);
    debug_printf("%s: mount returned %x\n", MODULE_NAME, res);
    if(res == FR_OK){
        fatfs_mounts[drive].mounted = true;
        fatfs_mounts[drive].mount_count = 1;;
    }
    return fatfs_map_error(res);
}

static FATError fatfs_unmount(FAT_UnmountRequest *req, int drive) {
    debug_printf("%s: Unmount drive %d, handle 0x%X\n", MODULE_NAME, drive, req->handle);
    if(fatfs_mounts[drive].mount_count > 0) {
        fatfs_mounts[drive].mount_count--;
    }
    if(fatfs_mounts[drive].mount_count == 0 && fatfs_mounts[drive].mounted){
        TCHAR path[5];
        snprintf(path, sizeof(path), "%d:", drive);
        FRESULT res = f_mount(0, path, 0);
        fatfs_mounts[drive].mounted = false;
        return fatfs_map_error(res);
    }
    return FAT_ERROR_OK;
}

static FATError fatfs_make_dir(FAT_MkdirRequest *req, int drive){
    char path_buf[512+4];
    snprintf(path_buf, sizeof(path_buf), "%d:%s", drive, req->path);
    FRESULT res = f_mkdir(path_buf);
    return fatfs_map_error(res);
}

static FATError fatfs_open_dir(FAT_OpenDirRequest *req, int drive){
    PathDIR *dp = ff_allocate_DIR();
    if(!dp) {
        return FAT_ERROR_OUT_OF_RESOURCES;
    }
    snprintf(dp->path, sizeof(dp->path), "%d:%s", drive, req->path);

    FRESULT res = f_opendir(&dp->dir, dp->path);
    debug_printf("%s: open_dir %p, %s, returned 0x%x\n", MODULE_NAME, dp, dp->path, res);
    if(res != FR_OK){
        ff_free_DIR(dp);
        dp = NULL;
    }
    *req->dirhandle_out_ptr = dp;
    return fatfs_map_error(res);
}

static void convert_filinfo_to_fsstat(FILINFO *info, FSStat *stat, int drive){
    FSStatFlags flags = 0x0c000000;
    if(info->fattrib & AM_DIR){
        flags |= FS_STAT_DIRECTORY;
    } else { //File
        flags |= FS_STAT_FILE;
    }
    stat->flags = flags;
    stat->mode = (info->fattrib & AM_RDO) ? 0x444:0x666;
    stat->size = info->fsize;

    FATFS *fs = fatfs_mounts[drive].fs;
    uint32_t cs = fs->csize * fs->ssize;
    uint32_t slack = cs - info->fsize % cs;
    stat->allocSize = info->fsize + slack;
}

static FATError fatfs_read_dir(FAT_ReadDirRequest *req, int drive){
    PathDIR *dp = *req->dp;
    FILINFO info;
    FRESULT res = f_readdir(&dp->dir, &info);
    debug_printf("%s: read_dir %p, %s, returned 0x%x\n", MODULE_NAME, dp, dp->path, res);
    if(res == FR_OK){
        if(info.fname[0] == 0){
            debug_printf("FAT_ERROR_END_OF_DIR");
            return FAT_ERROR_END_OF_DIR;
        }
        strncpy(req->entry->name, info.fname, sizeof(req->entry->name));
        convert_filinfo_to_fsstat(&info, &req->entry->info, drive);
    }
    return fatfs_map_error(res);
}

static FATError fatfs_close_dir(FAT_CloseDirRequst *req){
    PathDIR *dp = *req->dp;
    FRESULT res = f_closedir(&dp->dir);
    ff_free_DIR(dp);
    return fatfs_map_error(res);
}

static FATError fatfs_open_file(FAT_OpenFileRequest *req, int drive){
    BYTE mode = parse_mode_str(req->mode);
    PathFIL *fp = ff_allocate_FIL();
    if(!fp) {
        return FAT_ERROR_OUT_OF_RESOURCES;
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

static FATError fatfs_seek(FIL* fp, uint pos){
    FSIZE_t old_pos = f_tell(fp);
    FRESULT res = f_lseek(fp, pos);
    if(res != FR_OK)
        return fatfs_map_error(res);
    FSIZE_t curr_pos = f_tell(fp);
    if(curr_pos != pos){
        res = f_lseek(fp, old_pos);
        if(res != FR_OK)
            return fatfs_map_error(res);
        return FAT_ERROR_OUT_OF_RANGE;
    }
    return FAT_ERROR_OK;
}

static FATError fatfs_read_file(FAT_ReadFileRequest *req){
    PathFIL *fp = *(req->file);
    debug_printf("%s: ReadFile(%p, %d, %d, %u, %p (%s), 0x%x)", MODULE_NAME, req->buffer, req->size, req->count, req->pos,fp, fp->path, req->flags);

    if(req->flags & READ_REQUEST_WITH_POS){
        FATError error = fatfs_seek(&fp->fil, req->pos);
        if(error != FAT_ERROR_OK)
            return error;
    }
    UINT br;
    FRESULT res = f_read(&fp->fil, req->buffer, req->size * req->count, &br);
    if(res != FR_OK)
        return fatfs_map_error(res);
    debug_printf("read: %d bytes\n", br);
    return br / req->size;
}

static FATError fatfs_write_file(FAT_ReadFileRequest *req){
    PathFIL *fp = *req->file;
    FRESULT res;
    // TODO optimize: block seek for wo append, then we don't need to seek here for wo append
    if(fp->fil.flag & FA_OPEN_APPEND) {
        FILINFO *info;
        res = f_lseek(&fp->fil, (FSIZE_t)0xFFFFFFFFFFFFFFFF);
        if(res != FR_OK)
            return fatfs_map_error(res);
    }

    if(req->flags & READ_REQUEST_WITH_POS){
        FATError error = fatfs_seek(&fp->fil, req->pos);
        if(error != FAT_ERROR_OK)
            return error;
    }

    UINT bw;
    res = f_write(&fp->fil, req->buffer, req->size * req->count, &bw);
    if(res != FR_OK)
        return fatfs_map_error(res);

    return bw / req->size;
}

static FATError fatfs_stat_file(FAT_StatFileRequest *req, int drive) {
    PathFIL* fp = *req->fp;
    FILINFO info;
    debug_printf("%s: StatFile(%s)", MODULE_NAME, fp->path);
    FRESULT res = f_stat(fp->path, &info);
    if(res == FR_OK){
        convert_filinfo_to_fsstat(&info, req->stat, drive);
    }
    debug_printf("%s: StatFile(%s) -> size: %d res: %d\n", MODULE_NAME, fp->path, info.fsize, res);
    return fatfs_map_error(res);
}

static FATError fatfs_setpos_file(FAT_SetPosFileRequest *req){
    PathFIL *fp = *req->file;
    return fatfs_seek(&fp->fil, req->pos);
}

static FATError fatfs_close_file(FAT_CloseFileRequest *req){
    PathFIL *fp = *req->file;
    debug_printf("%s: CloseFile(%s)\n", MODULE_NAME, fp->path);
    FATError res = f_close(&fp->fil);
    ff_free_FIL(fp);
    return fatfs_map_error(res);
}

static FATError fatfs_remove(FAT_RemoveRequest *req, int drive){
    TCHAR path_buf[512+4];
    snprintf(path_buf, sizeof(path_buf), "%d:%s", drive, req->path);
    FRESULT res = f_unlink(path_buf);
    return fatfs_map_error(res);
}

static FATError fatfs_stat_fs(FAT_StatFSRequest *req, int drive) {
    debug_printf("%s: StatFS type %d\n", MODULE_NAME, req->type);
    TCHAR path_buf[512+4];
    FRESULT res;
    switch (req->type)
    {
        case FS_STAT_FREE_SPACE:
            DWORD nclst;
            FATFS *fs;
            snprintf(path_buf, sizeof(path_buf), "%d:", drive);
            res = f_getfree(path_buf, &nclst, &fs);
            if(res != FR_OK)
                return fatfs_map_error(res);
            uint64_t free_sector = nclst * fs->csize;
            *(uint64_t*)req->out_ptr = free_sector * fs->ssize;
            return FAT_ERROR_OK;
        case FS_STAT_GETSTAT:
            snprintf(path_buf, sizeof(path_buf), "%d:%s", drive, req->path);
            FILINFO info;
            debug_printf("%s: StatDir(%s)", MODULE_NAME, path_buf);
            res = f_stat(path_buf, &info);
            if(res == FR_OK){
                convert_filinfo_to_fsstat(&info, req->out_ptr, drive);
            }
            return fatfs_map_error(res);

        default:
            debug_printf("%s: Unimplemented FS Stat type: %d\n", MODULE_NAME, req->type);
            return FAT_ERROR_UNSUPPORTED_COMMAND;
    }
}

static FATError fatfs_message_dispatch(FAT_WorkMessage *message){

    // commands not requiring drive
    switch (message->command)
    {
        // case 0xb:
        //     break;
        case 0x09:
            return fatfs_close_dir(&message->request.close_dir);
        case 0x0b:
            return fatfs_read_file(&message->request.read_file);
        case 0x0c:
            return fatfs_write_file(&message->request.read_file);
        case 0x0e:
            return fatfs_setpos_file(&message->request.setpos_file);
        case 0x13:
            return fatfs_close_file(&message->request.close_file);
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
        case 0x03:
            return fatfs_unmount(&message->request.unmount, drive);
        case 0x04:
            return fatfs_make_dir(&message->request.make_dir, drive);
        case 0x06:
            return fatfs_open_dir(&message->request.open_dir, drive);
        case 0x07:
            return fatfs_read_dir(&message->request.read_dir, drive);
        case 0x0a:
            return fatfs_open_file(&message->request.open_file, drive);
        case 0x10:
            return fatfs_stat_file(&message->request.stat_file, drive);
        case 0x15:
            return fatfs_remove(&message->request.remove, drive);
        case 0x19:
            return fatfs_stat_fs(&message->request.stat_fs, drive);
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
