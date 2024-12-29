#include "fs_request.h"
#include "salio.h"
#include "wafel/utils.h"

static const char* MODULE_NAME = "SALFATFS";

static int fatfs_message_dispatch(FAT_WorkMessage *message){
    switch(message->command){
        case 0x02:
            return salio_mount(message->handle);
        case 0x03:
            //deattached
        case 0x0a:
            return TODO;
        case 0x2a:
        case 0x2c: 
            debug_printf("%s: Ignoring command %x\n", MODULE_NAME, message->command);
            return;    
        default:
            debug_printf("%s: Unknown command %x!!!! HALTING\n", MODULE_NAME, message->command);
            while(1);
    }
}

void salfatfs_process_message(FAT_WorkMessage *message){
    int ret = fatfs_message_dispatch(message);
    if(message->callback)
        message->callback(ret, message->calback_data);
}