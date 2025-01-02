#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <wafel/dynamic.h>
#include <wafel/ios_dynamic.h>
#include <wafel/utils.h>
#include <wafel/patch.h>
#include <wafel/ios/svc.h>
#include <wafel/trampoline.h>
#include <wafel/ios/ipc_types.h>
#include "fs_request.h"
#include "salfatfs.h"

const char *PLUGIN_NAME = "FATFS";


void print_hex(u8 *data, size_t len){
    for(int i=0; i<len; i++){
        if(i%16==0){
            debug_printf("\n%03X:", i);
        }
        debug_printf(" %02X", data[i]);
    }
    debug_printf("\n");
}

void print_request_hex(FAT_Request *req){
    debug_printf("Request:");
    print_hex((void*)req, 0x404);
}

void fsfat_init(FAT_WorkMessage *message){
    //fs_response *response = message->response;
    debug_printf("%s: Init\n", PLUGIN_NAME);
    // debug_printf("Message:");
    // print_hex((u8*)message, 0x24);
    //request is all zero
    //print_request_hex(&message->request);
    // response->set = 1;
    // response->retval = 0;
}

void fsfat_mount1(FAT_WorkMessage *message){
    debug_printf("%s: Mount 1\n", PLUGIN_NAME);
    // request is all zero
    //print_request_hex(&message->request);
    //debug_printf("Message:");
    //print_hex((u8*)message, 0x24);
}

void fsfat_mount0(FAT_WorkMessage *message){
    debug_printf("%s: Mount 0\n", PLUGIN_NAME);

    // debug_printf("Message:");
    // print_hex((u8*)message, 0x24);
    print_request_hex(&message->request);
}

void fsfat_open_file(FAT_WorkMessage *message){
    FAT_OpenFileRequest *req = &message->request.open_file;
    FSSALHandle sal_hanlde = message->volume_handle;
    debug_printf("%s: OpenFile(%s, %s) handle: %04x\n", PLUGIN_NAME, req->path, req->mode, sal_hanlde);
    //print_request_hex(&message->request);
}

void fsfat_command_switch(FAT_WorkMessage *message){
    switch(message->command){
        case 0x02:
            return fsfat_mount0(message);
        case 0x03:
            //deattached
        case 0x0a:
            return fsfat_open_file(message);
        case 0x2a:
            return fsfat_init(message);
        case 0x2c: 
            return fsfat_mount1(message);    
        default:
            //debug_printf("%s: Unknown command %x!!!!\n", PLUGIN_NAME, message->command);
    }
}

void fsfat_hook(trampoline_state *regs){
    FAT_WorkMessage *message = (FAT_WorkMessage*)regs->r[7];
    
    debug_printf("FSFAT_ActiveDeviceHandle: %X\n", *((int*)0x10bb7e4c));
    debug_printf("FATFS Message:");
    print_hex((u8*)message, 0x24);

    fsfat_command_switch(message);

    salfatfs_process_message(message);
    regs->lr = 0x1078a664;
    
    // debug_printf("FSFAT Request: %p", request);
    // for(int i=0; i<0x428; i++){
    //     if(i%16==0){
    //         debug_printf("\n%03X:", i);
    //     }
    //     debug_printf(" %02X", request[i]);
    // }
    // debug_printf("\n");


}

uint fsfat_device_attach_hook(uint fs_handle, uint existing_volume, FSVolumeArgs *volume_args, int r3, uint (*add_volume)(uint, uint, FSVolumeArgs*)){
    uint device_handle = volume_args->device_handle;
    uint volume_handle = add_volume(fs_handle, existing_volume, volume_args);
    debug_printf("%s: Attached volume %08X on device %08X\n", PLUGIN_NAME, volume_handle, device_handle);
    salfatfs_add_volume(volume_handle, device_handle);
    return volume_handle; 
}

void fsfat_ret_hook(trampoline_state *regs){
    FAT_WorkMessage *message = (FAT_WorkMessage*)regs->r[7];
    int ret = regs->r[5];
    debug_printf("%s: ret: %x, callback: %p, worker->ret: %x, worker->set: %x\n", PLUGIN_NAME, ret, message->callback, message->worker->retval, message->worker->set);
}

void fsfat_init_res_hook(trampoline_state *regs){
    int ret = regs->r[0];
    debug_printf("%s: MsgWorkThread Init returend: %x\n", PLUGIN_NAME, ret);
}

void fswfs_hook(trampoline_state *regs){
    FAT_WorkMessage *message = (FAT_WorkMessage*)regs->r[8];
    debug_printf("WFS: %08x %08x %02x\n", message->volume_handle, message->worker, message->command);
    fsfat_command_switch(message);
}

int raw_read_aligment_hook(void *r0, int r1, int r2, int r3, int (*org_func)(void*, int)) {
    debug_printf("%s: FSSAL_RawRead calling %p with (%p, 0x%x)\n", PLUGIN_NAME, org_func, r0, r1);
    int ret = org_func(r0, r1);
    debug_printf("returned 0x%x\n", ret);
    return ret;
}

void raw_read_aligment_hook2(trampoline_state *regs) {
    debug_printf("%s: FSSAL_RawRead calling FUN_107f6ff8 with (%p, 0x%x)\n", PLUGIN_NAME, regs->r[0], regs->r[1]);

}


// This fn runs before everything else in kernel mode.
// It should be used to do extremely early patches
// (ie to BSP and kernel, which launches before MCP)
// It jumps to the real IOS kernel entry on exit.
__attribute__((target("arm")))
void kern_main()
{
    // Make sure relocs worked fine and mappings are good
    debug_printf("we in here fatfs plugin kern %p\n", kern_main);

    debug_printf("init_linking symbol at: %08x\n", wafel_find_symbol("init_linking"));

    trampoline_hook_before(0x1078a688, fsfat_hook);
    trampoline_blreplace(0x1078a388, fsfat_device_attach_hook);

    trampoline_hook_before(0x1078a8dc, fsfat_ret_hook);
    trampoline_hook_before(0x1078a548, fsfat_init_res_hook);
    
    //trampoline_blreplace(0x10732c1c, raw_read_aligment_hook);
    //trampoline_hook_before(0x10732c1c, raw_read_aligment_hook2);

    //trampoline_hook_before(0x1073e9b4, fswfs_hook);

}

// This fn runs before MCP's main thread, and can be used
// to perform late patches and spawn threads under MCP.
// It must return.
void mcp_main()
{

}
