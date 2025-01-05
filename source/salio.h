#include "fs_request.h"
#include "fatfs/ff.h"

#define SALIO_ALIGNMENT 32

void salio_set_dev_handle(int index, uint dev_handle);