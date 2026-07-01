/* IKOS Orthogonal Persistence v2 - Open-file table serialization (#141)
 *
 * Serializes a process's open files into a portable blob and rebuilds them on
 * restore. Files are the easy case for persistence: their bytes live in the
 * persistent filesystem, so we persist the descriptor (number, offset, flags,
 * kind, path) and on restore reopen the backing file by path and seek to the
 * saved offset. Non-persistable kinds (sockets, DMA, devices) are not reopened;
 * they are severed via the external-state policy (#118/#128).
 *
 * Two layers, mirroring the process-table work (#140):
 *   - a pure, dependency-free serialize/deserialize/validate core (host-tested);
 *   - kernel capture/restore adapters (declared here, defined in the kernel sync
 *     module) that snapshot proc->fds[] and reopen via the VFS.
 *
 * See docs/architecture/orthogonal-persistence-v2.md §5.
 */

#ifndef CHECKPOINT_FILETABLE_H
#define CHECKPOINT_FILETABLE_H

#include <stdint.h>
#include <stdbool.h>

#define CHECKPOINT_FILETABLE_MAGIC   0x494B46494C455431ULL /* "IKFILET1" */
#define CHECKPOINT_FILETABLE_VERSION 1
#define CHECKPOINT_FILETABLE_MAX     64  /* MAX_OPEN_FILES */
#define CHECKPOINT_FILE_PATH_MAX     128 /* per-descriptor backing path */

/* Portable record for one open descriptor. */
typedef struct {
    uint32_t fd;
    uint32_t flags;
    uint64_t offset;
    uint32_t kind;     /* extstate_kind_t: regular file persistable, others severed */
    uint32_t reserved;
    char     path[CHECKPOINT_FILE_PATH_MAX]; /* NUL-terminated; empty if unknown */
} checkpoint_file_record_t;

typedef struct {
    uint32_t pid;
    uint32_t count;
    checkpoint_file_record_t records[CHECKPOINT_FILETABLE_MAX];
} checkpoint_filetable_t;

/* ----- Pure serialization core ----- */

uint32_t checkpoint_filetable_serialize(const checkpoint_filetable_t* t,
                                        void* buf, uint32_t bufsize);
bool     checkpoint_filetable_deserialize(checkpoint_filetable_t* t,
                                          const void* buf, uint32_t size);

/* Referential / shape check: count <= MAX, fds unique, and every record's path
 * is NUL-terminated within CHECKPOINT_FILE_PATH_MAX. */
bool checkpoint_filetable_validate(const checkpoint_filetable_t* t);

const checkpoint_file_record_t* checkpoint_filetable_find(const checkpoint_filetable_t* t,
                                                          uint32_t fd);

uint32_t checkpoint_filetable_blob_size(uint32_t count);

/* ----- Kernel capture/restore adapters (kernel sync module) ----- */
#define CHECKPOINT_TAG_FILETABLE 2

/* Snapshot one process's open-file table into a kernel-state blob (#139).
 * Returns 0 on success, negative on error. */
int checkpoint_capture_filetable(uint32_t pid);

/* Rebuild a process's open files from a serialized blob: reopen persistable
 * files by path and seek to the saved offset; sever non-persistable handles.
 * Returns 0 on success, negative on error. */
int checkpoint_restore_filetable(const void* blob, uint32_t size);

#endif /* CHECKPOINT_FILETABLE_H */
