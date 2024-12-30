#include "fs_request.h"
#include "salio.h"
#include "fatfs/ff.h"
#include <wafel/utils.h>
#include <stdio.h>
#include <stdlib.h>

static const char* MODULE_NAME = "SALFATFS";

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

static int fatfs_mount(FSSALHandle sal_handle){
    int index = salio_get_drive_number(sal_handle);
    if(index<0)
        index = salio_add_sal_handle(sal_handle);
    if(index<0)
        return -1;

    TCHAR path[5];
    snprintf(path, sizeof(path), "%d:", index);
    
    debug_printf("%s: Mounting index: %d, path: %s, salhandle: 0x%08x\n", MODULE_NAME, index, path, sal_handle);

    FRESULT res = f_mount(salio_get_fatfs(index), path, 0);
    if(res != FR_OK){
        salio_remove_sal_volume(index);
    }
    return res;
}

static FRESULT fatfs_open_file(FAT_OpenFileRequest *req, int drive){
    BYTE mode = parse_mode_str(req->mode);
    TCHAR path_buff[sizeof(req->path)+4];
    snprintf(path_buff, sizeof(path_buff), "%d:%s", drive, req->path);
    FIL *fp = malloc(sizeof(FIL));
    FRESULT res = f_open(fp, path_buff, mode);
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
    case 0xb:
        break;
    
    }

    // commands where drive can be invalid
    int drive = salio_get_drive_number(message->handle);
    switch(message->command){
        case 0x02:
            return fatfs_mount(message->handle);
        case 0x2a:
        case 0x2c: 
            debug_printf("%s: Ignoring command %x\n", MODULE_NAME, message->command);
            return;    
    }

    if(drive<0)
        return -1;

    //commands requirering a valid drive
    switch(message->command){
        case 0x03:
            //deattached
        case 0x0a:
            return fatfs_open_file(&message->request.open_file, message->handle);
        case 0x2a:
        case 0x2c: 
            debug_printf("%s: Ignoring command %x\n", MODULE_NAME, message->command);
            return;    
    }

    debug_printf("%s: Unknown command %x!!!! HALTING\n", MODULE_NAME, message->command);
    while(1);
}

void salfatfs_process_message(FAT_WorkMessage *message){
    int ret = fatfs_message_dispatch(message);
    if(message->callback)
        message->callback(ret, message->calback_data);
}