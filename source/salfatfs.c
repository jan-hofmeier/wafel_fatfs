#include "fs_request.h"
#include "salio.h"
#include "fatfs/ff.h"
#include <wafel/utils.h>
#include <wafel/ios/memory.h>
#include <stdio.h>
#include <stdlib.h>

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

FIL* ff_allocate_FIL(void){
    FIL *fp = malloc_local(sizeof(FIL));
    if(!fp)
        return fp;
    fp->buf = iosAllocAligned(HEAPID_LOCAL, FF_MAX_SS, 0x20);
    if(!fp->buf){
        free_local(fp);
        return NULL;
    }
    return fp;
}

void ff_free_FIL(FIL *fp){
    free_local(fp->buf);
    free_local(fp);
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

static int fatfs_mount(int drive){
    if(fatfs_mounts[drive].mounted){
        fatfs_mounts[drive].mount_count++;
    }

    TCHAR path[5];
    snprintf(path, sizeof(path), "%d:", drive);
    
    debug_printf("%s: Mounting index: %d, path: %s, volume_handle: 0x%08x\n", MODULE_NAME, drive, path, fatfs_mounts[drive].volume_handle);

    if(!fatfs_mounts[drive].fs){
        fatfs_mounts[drive].fs = ff_allocate_FATFS();
        if(!fatfs_mounts[drive].fs)
            return -1;
    }

    FRESULT res = f_mount(fatfs_mounts[drive].fs, path, 0);
    debug_printf("%s: mount returned %x\n", MODULE_NAME, res);
    if(res != FR_OK){
        return -1;
    }
    fatfs_mounts[drive].mounted = true;
    fatfs_mounts[drive].mount_count = 1;
    return res;
}

static FRESULT fatfs_open_file(FAT_OpenFileRequest *req, int drive){
    BYTE mode = parse_mode_str(req->mode);
    TCHAR path_buff[sizeof(req->path)+4];
    snprintf(path_buff, sizeof(path_buff), "%d:%s", drive, req->path);
    FIL *fp = ff_allocate_FIL();
    FRESULT res = f_open(fp, path_buff, mode);
    debug_printf("%s: open_file %p, %s, 0x%x returned 0x%x\n", MODULE_NAME, fp, path_buff, mode, res);
    if(res != FR_OK){
        free(fp);
        fp = NULL;
    }
    *req->filehandle_out_ptr = fp;
    return res;
}

static int fatfs_message_dispatch(FAT_WorkMessage *message){

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
    }

    debug_printf("%s: Unknown command %x!!!! HALTING\n", MODULE_NAME, message->command);
    while(1);
    return -1;
}

void salfatfs_process_message(FAT_WorkMessage *message){
    int ret = fatfs_message_dispatch(message);
    if(message->callback)
        message->callback(ret, message->calback_data);
}