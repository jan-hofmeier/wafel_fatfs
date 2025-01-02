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
    bool mounted;
    int mount_count;

} fatfs_mounts[FF_VOLUMES] = {};

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

static int fatfs_mount(FSSALHandle sal_handle, bool premount){
    int index = salio_get_drive_number(sal_handle);
    if(index<0)
        index = salio_add_sal_handle(sal_handle);
    if(index<0)
        return -1;

    TCHAR path[5];
    snprintf(path, sizeof(path), "%d:", index);
    
    debug_printf("%s: Mounting index: %d, path: %s, salhandle: 0x%08x\n", MODULE_NAME, index, path, sal_handle);

    FRESULT res = f_mount(salio_get_fatfs(index), path, 0);
    debug_printf("%s: mount returned %x\n", MODULE_NAME, res);
    if(res != FR_OK){
        salio_remove_sal_volume(index);
    }
    return res;
}

static FRESULT fatfs_open_file(FAT_OpenFileRequest *req, int drive){
    BYTE mode = parse_mode_str(req->mode);
    TCHAR path_buff[sizeof(req->path)+4];
    snprintf(path_buff, sizeof(path_buff), "%d:%s", drive, req->path);
    FIL *fp = malloc_cross_process(sizeof(FIL));
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

    // commands where drive can be invalid
    int drive = salio_get_drive_number(message->handle);
    switch(message->command){
        case 0x02:
            return fatfs_mount(message->handle, false);
   
    }

    if(drive<0)
        return -1;

    //commands requirering a valid drive
    switch(message->command){
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