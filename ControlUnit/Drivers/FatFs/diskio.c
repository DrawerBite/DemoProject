#include <HighAbstraction/SDCardSPI.h>
#include "diskio.h"
#include "ff.h"

DSTATUS disk_status (BYTE pdrv) {
    return 0; // OK
}

DSTATUS disk_initialize (
    BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    // Запускаем инициализацию и сохраняем её результат
    uint8_t res = sd_init();

    if (res == 0) {
        return 0;
    } else {
        return STA_NOINIT;
    }
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    for(; count > 0; count--, sector++, buff += 512) {
        if (sd_read_sector(sector, buff) != 0) return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    for(; count > 0; count--, sector++, buff += 512) {
        if (sd_write_sector(sector, buff) != 0) return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff) {
    return RES_OK;
}

DWORD get_fattime (void) {
    return 0; // Время 00:00:00
}
