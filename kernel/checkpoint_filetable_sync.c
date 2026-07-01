/* IKOS Orthogonal Persistence v2 - Open-file table capture/restore adapter (#141)
 *
 * Bridges a process's proc->fds[] to the portable serialization core
 * (checkpoint_filetable.c), the external-state policy (#128), and the VFS.
 * Kernel only; the host tests cover the pure core, this adapter is validated by
 * the build.
 */

#include "checkpoint_filetable.h"
#include "checkpoint.h"
#include "checkpoint_extstate.h"
#include "process_manager.h"   /* process_t, file_descriptor_t, MAX_OPEN_FILES, pm */
#include "vfs.h"               /* vfs_open, vfs_lseek */
#include <stddef.h>

extern void* kmalloc(size_t size);
extern void  kfree(void* ptr);

#define SEEK_SET_ 0

int checkpoint_capture_filetable(uint32_t pid) {
    process_t* proc = process_get_by_pid((pid_t)pid);
    if (!proc) {
        return -1;
    }

    checkpoint_filetable_t* t = (checkpoint_filetable_t*)kmalloc(sizeof(*t));
    if (!t) {
        return -1;
    }
    t->pid = pid;
    t->count = 0;

    for (int i = 0; i < MAX_OPEN_FILES && t->count < CHECKPOINT_FILETABLE_MAX; i++) {
        file_descriptor_t* fd = &proc->fds[i];
        if (fd->fd < 0) {
            continue; /* slot not in use */
        }
        checkpoint_file_record_t* r = &t->records[t->count++];
        r->fd = (uint32_t)fd->fd;
        r->flags = fd->flags;
        r->offset = fd->offset;
        r->kind = (uint32_t)extstate_kind_from_fd_flags(fd->flags);
        r->reserved = 0;
        /* The descriptor does not currently store its backing path; leave it
         * empty until the VFS exposes it (records still carry offset/flags/kind
         * so non-persistable handles are severed and metadata is restored). */
        for (int k = 0; k < CHECKPOINT_FILE_PATH_MAX; k++) {
            r->path[k] = '\0';
        }
    }

    uint32_t bufsize = checkpoint_filetable_blob_size(t->count);
    uint8_t* buf = (uint8_t*)kmalloc(bufsize);
    if (!buf) {
        kfree(t);
        return -1;
    }

    int rc = -1;
    uint32_t written = checkpoint_filetable_serialize(t, buf, bufsize);
    if (written) {
        rc = checkpoint_capture_kernel_blob(CHECKPOINT_TAG_FILETABLE, buf, written);
    }
    kfree(buf);
    kfree(t);
    return rc;
}

int checkpoint_restore_filetable(const void* blob, uint32_t size) {
    checkpoint_filetable_t* t = (checkpoint_filetable_t*)kmalloc(sizeof(*t));
    if (!t) {
        return -1;
    }
    if (!checkpoint_filetable_deserialize(t, blob, size) ||
        !checkpoint_filetable_validate(t)) {
        kfree(t);
        return -1;
    }

    process_t* proc = process_get_by_pid((pid_t)t->pid);
    if (!proc) {
        kfree(t);
        return -1;
    }

    for (uint32_t i = 0; i < t->count; i++) {
        checkpoint_file_record_t* r = &t->records[i];

        /* Find the matching descriptor slot, or the first free one. */
        file_descriptor_t* slot = NULL;
        for (int j = 0; j < MAX_OPEN_FILES; j++) {
            if (proc->fds[j].fd == (int)r->fd) { slot = &proc->fds[j]; break; }
        }
        if (!slot) {
            for (int j = 0; j < MAX_OPEN_FILES; j++) {
                if (proc->fds[j].fd < 0) { slot = &proc->fds[j]; break; }
            }
        }
        if (!slot) {
            continue;
        }

        slot->fd = (int)r->fd;
        slot->offset = r->offset;
        slot->flags = r->flags;
        slot->file_data = NULL;

        if (extstate_survives_checkpoint((extstate_kind_t)r->kind) && r->path[0] != '\0') {
            /* Regular file: reopen by path and seek to the saved offset. (The
             * VFS-level fd is reconciled with r->fd by a later VFS change.) */
            int vfd = vfs_open(r->path, r->flags, 0);
            if (vfd >= 0) {
                vfs_lseek(vfd, r->offset, SEEK_SET_);
            }
        } else {
            /* Non-persistable handle (socket/DMA/device), or a file whose path
             * we could not capture: sever it so the app re-establishes it. */
            extstate_fd_set_kind(&slot->flags, (extstate_kind_t)r->kind);
            extstate_sever_fd(&slot->flags);
        }
    }

    kfree(t);
    return 0;
}
