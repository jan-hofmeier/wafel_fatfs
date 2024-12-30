#include "fs_request.h"
#include "fatfs/ff.h"

int salio_add_sal_handle(FSSALHandle sal_handle);
int salio_get_drive_number(FSSALHandle sal_handle);
void salio_remove_sal_volume(int pdrv);
FATFS* salio_get_fatfs(int pdrv);