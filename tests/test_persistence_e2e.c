/* End-to-end orthogonal-persistence demo / test (#119).
 *
 * The "yank power, it remembers" proof, exercised against the REAL checkpoint
 * engine + on-disk snapshot store using a FILE-BACKED block device. Because the
 * disk is a real file, the only state that survives between separate process
 * invocations is what reached the disk, so re-running this program against the
 * same file genuinely models a power cycle: in-memory state is gone; the kernel
 * resumes from the last committed checkpoint.
 *
 * A standalone counter program (see user/persistent_counter.c) just increments
 * a counter; orthogonal persistence makes it durable with no save/load code.
 * Here we drive the same idea through the kernel's checkpoint path:
 *   take() -> capture page -> writeback() -> commit   (each "checkpoint")
 *   restore()                                          (on the next boot)
 *
 * Usage:
 *   test_persistence_e2e <diskfile> run <N>       increment N times, checkpoint
 *                                                  each step, then exit
 *   test_persistence_e2e <diskfile> verify <V>    restore and assert counter==V
 *   test_persistence_e2e <diskfile> show          restore and print the counter
 *
 * Build: gcc -I../include -o test_persistence_e2e test_persistence_e2e.c \
 *            ../kernel/checkpoint.c ../kernel/snapshot_store.c \
 *            ../kernel/checkpoint_extstate.c ../kernel/checkpoint_barrier.c
 */

#include <stdint.h>
#include <stdbool.h>

/* Avoid libc headers (their <sys/types.h> ssize_t clashes with IKOS vfs.h).
 * Declare exactly the libc entry points we use. */
typedef __SIZE_TYPE__ size_t;
typedef struct _IO_FILE FILE;
extern FILE* fopen(const char*, const char*);
extern size_t fread(void*, size_t, size_t, FILE*);
extern size_t fwrite(const void*, size_t, size_t, FILE*);
extern int   fseek(FILE*, long, int);
extern int   fflush(FILE*);
extern int   fclose(FILE*);
extern int   printf(const char*, ...);
extern long  atol(const char*);
extern int   strcmp(const char*, const char*);
extern void* malloc(size_t);
extern void  free(void*);
extern void* aligned_alloc(size_t, size_t);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
#define SEEK_SET 0

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"   /* checkpoint + snapshot_store + fat_block_device_t */

/* ----- Engine link stubs. Empty process list => take() opens an epoch and the
 * writeback clean-page walk finds nothing; we drive the counter page through
 * capture explicitly below. ----- */
pte_t* vmm_get_page_table(vm_space_t* s, uint64_t a, int l, bool c) {
    (void)s; (void)a; (void)l; (void)c; return 0;
}
vm_space_t* vmm_get_current_space(void) { return 0; }
void vmm_flush_tlb_page(uint64_t a) { (void)a; }
uint64_t vmm_get_physical_addr(vm_space_t* s, uint64_t a) { (void)s; (void)a; return 0; }
vm_space_t* vmm_create_address_space(uint32_t pid) { (void)pid; return 0; }
uint64_t vmm_alloc_page(void) { return 0; }
int vmm_map_page(vm_space_t* s, uint64_t v, uint64_t p, uint32_t f) {
    (void)s; (void)v; (void)p; (void)f; return 0;
}
struct process;
int pm_get_process_list(uint32_t* p, uint32_t m, uint32_t* c) {
    (void)p; (void)m; if (c) *c = 0; return 0;
}
struct process* pm_get_process(uint32_t pid) { (void)pid; return 0; }
/* Process-reconstruction stubs (checkpoint_register_kernel, unused here). */
struct process* process_get_by_pid(int pid) { (void)pid; return 0; }
struct process* process_create(const char* a, const char* b) { (void)a; (void)b; return 0; }
int pm_table_add_process(struct process* p) { (void)p; return 0; }
int scheduler_add_process(struct process* p) { (void)p; return 0; }

/* ----- File-backed block device ----- */
#define DISK_SECTORS 2048   /* 1 MiB image */
static int file_read(void* dev, uint32_t sector, uint32_t count, void* buf) {
    FILE* f = (FILE*)dev;
    if ((uint64_t)sector + count > DISK_SECTORS) return -1;
    if (fseek(f, (long)sector * SNAPSHOT_SECTOR_SIZE, SEEK_SET) != 0) return -1;
    return fread(buf, 1, (size_t)count * SNAPSHOT_SECTOR_SIZE, f)
               == (size_t)count * SNAPSHOT_SECTOR_SIZE ? 0 : -1;
}
static int file_write(void* dev, uint32_t sector, uint32_t count, const void* buf) {
    FILE* f = (FILE*)dev;
    if ((uint64_t)sector + count > DISK_SECTORS) return -1;
    if (fseek(f, (long)sector * SNAPSHOT_SECTOR_SIZE, SEEK_SET) != 0) return -1;
    if (fwrite(buf, 1, (size_t)count * SNAPSHOT_SECTOR_SIZE, f)
            != (size_t)count * SNAPSHOT_SECTOR_SIZE) return -1;
    fflush(f); /* push to disk: a "power cut" after this keeps the data */
    return 0;
}

/* The counter lives at offset 0 of one persisted page. */
#define COUNTER_PID   1
#define COUNTER_VADDR 0x400000ULL

static int recover_counter(void* ctx, const snapshot_page_record_t* rec) {
    uint64_t* out = (uint64_t*)ctx;
    memcpy(out, rec->page_data, sizeof(*out)); /* counter is at offset 0 */
    return CHECKPOINT_OK;
}

/* Restore the latest checkpointed counter (0 if none). Leaves the engine epoch
 * set to the restored epoch so new checkpoints advance monotonically. */
static uint64_t load_counter(snapshot_store_t* store) {
    checkpoint_init();
    uint64_t counter = 0;
    checkpoint_restore(store, recover_counter, &counter); /* no-op if no checkpoint */
    return counter;
}

/* Take one checkpoint of the counter page: mark -> capture -> writeback -> commit. */
static int checkpoint_counter(snapshot_store_t* store, uint8_t* page) {
    checkpoint_take();                       /* advance + open the epoch */
    pte_t pte = 0x10000ULL | PAGE_PRESENT | PAGE_SNAPSHOT_COW;
    int rc = checkpoint_capture_page(COUNTER_PID, COUNTER_VADDR, page, &pte);
    if (rc != CHECKPOINT_OK) return rc;
    return checkpoint_writeback(store);      /* stream to disk + commit */
}

static FILE* open_disk(const char* path) {
    FILE* f = fopen(path, "r+b");            /* existing image: keep contents */
    if (f) return f;
    f = fopen(path, "w+b");                  /* fresh image: preallocate zeros */
    if (!f) return 0;
    uint8_t zero[SNAPSHOT_SECTOR_SIZE];
    memset(zero, 0, sizeof(zero));
    for (int i = 0; i < DISK_SECTORS; i++) fwrite(zero, 1, sizeof(zero), f);
    fflush(f);
    return f;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: %s <diskfile> run <N> | verify <V>\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    const char* mode = argv[2];

    FILE* disk = open_disk(path);
    if (!disk) { printf("cannot open %s\n", path); return 2; }

    fat_block_device_t dev = {0};
    dev.read_sectors = file_read;
    dev.write_sectors = file_write;
    dev.sector_size = SNAPSHOT_SECTOR_SIZE;
    dev.total_sectors = DISK_SECTORS;
    dev.private_data = disk;

    snapshot_store_t store;
    if (snapshot_store_init(&store, &dev, CHECKPOINT_STORE_BASE_SECTOR,
                            CHECKPOINT_STORE_SLOT_SECTORS) != SNAPSHOT_OK) {
        printf("store init failed\n"); fclose(disk); return 2;
    }

    /* Format only when the image holds no valid checkpoint (first boot). */
    snapshot_reader_t probe;
    if (snapshot_store_load(&store, &probe) != SNAPSHOT_OK) {
        snapshot_store_format(&store);
    }

    int ret = 0;
    if (strcmp(mode, "run") == 0) {
        long n = atol(argv[3]);
        uint64_t counter = load_counter(&store);      /* resume from last boot */
        uint8_t* page = (uint8_t*)aligned_alloc(SNAPSHOT_PAGE_SIZE, SNAPSHOT_PAGE_SIZE);
        memset(page, 0, SNAPSHOT_PAGE_SIZE);
        printf("  [run] resumed counter = %lu\n", (unsigned long)counter);
        for (long i = 0; i < n; i++) {
            counter++;
            memcpy(page, &counter, sizeof(counter));
            if (checkpoint_counter(&store, page) != CHECKPOINT_OK) {
                printf("  [run] checkpoint failed at %lu\n", (unsigned long)counter);
                ret = 2; break;
            }
        }
        printf("  [run] counter now = %lu (then 'power is cut')\n", (unsigned long)counter);
        free(page);
    } else if (strcmp(mode, "verify") == 0) {
        uint64_t expect = (uint64_t)atol(argv[3]);
        uint64_t counter = load_counter(&store);
        if (counter == expect) {
            printf("  [verify] counter = %lu  == expected %lu  -> REMEMBERED\n",
                   (unsigned long)counter, (unsigned long)expect);
            ret = 0;
        } else {
            printf("  [verify] counter = %lu  != expected %lu  -> FORGOT\n",
                   (unsigned long)counter, (unsigned long)expect);
            ret = 1;
        }
    } else if (strcmp(mode, "show") == 0) {
        uint64_t counter = load_counter(&store);
        printf("  [show] counter = %lu\n", (unsigned long)counter);
        ret = 0;
    } else {
        printf("unknown mode '%s'\n", mode);
        ret = 2;
    }

    fclose(disk);
    return ret;
}
