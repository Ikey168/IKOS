/* IKOS Orthogonal Persistence v2 - Disk (IDE) driver re-attach adapter (#144)
 *
 * Wires the pure reconcile core (checkpoint_disk.c) to the real IDE driver and
 * registers the boot drive that backs the checkpoint store into the global
 * driver registry (#143). Compile-checked freestanding; exercised in QEMU.
 */

#include "checkpoint_disk.h"
#include "checkpoint_driver.h"
#include "ide_driver.h"

/* Binding for the one IDE drive that holds the checkpoint store. */
typedef struct {
    ide_device_t* ide;
    uint8_t       drive;
    uint64_t      store_lba;
} checkpoint_disk_dev_t;

static int disk_flush(void* ctx) {
    checkpoint_disk_dev_t* d = (checkpoint_disk_dev_t*)ctx;
    return ide_flush_cache(d->ide, d->drive);
}

static int disk_reprobe(void* ctx) {
    checkpoint_disk_dev_t* d = (checkpoint_disk_dev_t*)ctx;
    return ide_identify_drives(d->ide);
}

static int disk_read_sector(void* ctx, uint64_t lba, void* buf512) {
    checkpoint_disk_dev_t* d = (checkpoint_disk_dev_t*)ctx;
    return ide_read_sectors(d->ide, d->drive, lba, 1, buf512);
}

static checkpoint_disk_ops_t disk_ops_for(checkpoint_disk_dev_t* d) {
    checkpoint_disk_ops_t o;
    o.flush       = disk_flush;
    o.reprobe     = disk_reprobe;
    o.read_sector = disk_read_sector;
    o.ctx         = d;
    o.store_lba   = d->store_lba;
    return o;
}

/* persist_ops (#143) thunks: rebuild the ops vtable and call the pure core. */
static int disk_persist_quiesce(void* dev) {
    checkpoint_disk_dev_t* d = (checkpoint_disk_dev_t*)dev;
    checkpoint_disk_ops_t o = disk_ops_for(d);
    return checkpoint_disk_quiesce(&o);
}

static int disk_persist_reattach(void* dev) {
    checkpoint_disk_dev_t* d = (checkpoint_disk_dev_t*)dev;
    checkpoint_disk_ops_t o = disk_ops_for(d);
    return checkpoint_disk_reattach(&o);
}

/* One static binding: the checkpoint store lives on a single boot drive. */
static checkpoint_disk_dev_t g_disk_dev;

int checkpoint_disk_register(void* ide, uint8_t drive, uint64_t store_lba) {
    if (!ide) {
        return -1;
    }
    g_disk_dev.ide       = (ide_device_t*)ide;
    g_disk_dev.drive     = drive;
    g_disk_dev.store_lba = store_lba;

    static const checkpoint_persist_ops_t ops = {
        disk_persist_quiesce, disk_persist_reattach, true
    };
    return checkpoint_driver_register(checkpoint_driver_global(), &g_disk_dev, &ops);
}
