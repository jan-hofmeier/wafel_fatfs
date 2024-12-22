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


void fsfat_hook(trampoline_state *regs){
    FAT_WorkMessage *request = (u8*)regs->r[7];
    debug_printf("FSFAT Request: %p", request);
    for(int i=0; i<0x428; i++){
        if(i%16==0){
            debug_printf("\n%03X:", i);
        }
        debug_printf(" %02X", request[i]);
    }
    debug_printf("\n");


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

}

// This fn runs before MCP's main thread, and can be used
// to perform late patches and spawn threads under MCP.
// It must return.
void mcp_main()
{

}
