/* IKOS Orthogonal Persistence - IDE durable-store boot wiring (#201)
 *
 * See include/checkpoint_ide_boot.h. Initializes the primary IDE controller,
 * identifies its drives, and binds the first present one as the persistence
 * backing device via checkpoint_ide_bind (#130). Kept separate from
 * checkpoint_ide.c (the pure adapter) so that core stays host-testable while
 * this hardware bring-up is verified by the freestanding compile check.
 */

#include "checkpoint_ide_boot.h"
#include "checkpoint_ide.h"   /* checkpoint_ide_bind, checkpoint_ide_binding_t */
#include "ide_driver.h"       /* ide_device_t, ide_init_controller, ... */
#include <stddef.h>

/* Static storage: the controller, the binding, and the block device all outlive
 * this call and back the persistence store for the rest of the boot. */
static ide_device_t             g_ide;
static checkpoint_ide_binding_t g_binding;
static fat_block_device_t       g_bdev;

fat_block_device_t* checkpoint_ide_boot_bind(void) {
    /* Standard primary IDE channel: 0x1F0 / 0x3F6, IRQ 14. */
    if (ide_init_controller(&g_ide, IDE_PRIMARY_BASE, IDE_PRIMARY_CTRL, 14)
            != IDE_SUCCESS) {
        return NULL;
    }
    /* Best-effort IDENTIFY; presence is checked per drive below regardless. */
    ide_identify_drives(&g_ide);

    for (uint8_t drive = 0; drive < 2; drive++) {
        if (!ide_drive_present(&g_ide, drive)) {
            continue;
        }
        uint64_t total = g_ide.drives[drive].total_sectors;
        if (total == 0) {
            continue; /* no usable capacity reported */
        }
        if (checkpoint_ide_bind(&g_bdev, &g_binding, &g_ide, drive, total)) {
            return &g_bdev;
        }
    }
    return NULL; /* no IDE drive: caller falls back to the RAM disk */
}
