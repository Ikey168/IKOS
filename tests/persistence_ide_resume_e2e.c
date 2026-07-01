/* In-QEMU-style persistence resume over the DURABLE IDE store (#202).
 *
 * The "yank power, it remembers" proof, driven through the REAL checkpoint
 * engine and on-disk snapshot store, with the block device produced by the
 * checkpoint_ide binding (#130/#201) over a file-backed "IDE disk". Because the
 * disk is a real file and the IDE binding is exactly what the kernel wires at
 * boot, re-running this program against the same image genuinely models a power
 * cycle on the durable IDE store: in-memory state is gone; the kernel resumes
 * from the last committed checkpoint on the disk.
 *
 * This complements test_persistence_e2e (which uses a raw file device): here the
 * store rides the same IDE adapter the booted kernel uses, so it proves the
 * resume comes from the durable IDE-backed store.
 *
 *   persistence_ide_resume_e2e <diskfile> run <N>     increment N times,
 *                                                      checkpoint each step
 *   persistence_ide_resume_e2e <diskfile> verify <V>  reboot, assert counter==V
 *   persistence_ide_resume_e2e <diskfile> show        reboot, print the counter
 *
 * Build: gcc -I../include -o persistence_ide_resume_e2e \
 *          persistence_ide_resume_e2e.c ../kernel/checkpoint.c \
 *          ../kernel/snapshot_store.c ../kernel/checkpoint_extstate.c \
 *          ../kernel/checkpoint_barrier.c ../kernel/checkpoint_ide.c
 */

#include <stdint.h>
#include <stdbool.h>

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
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
#define SEEK_SET 0

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"       /* checkpoint + snapshot_store + fat_block_device_t */
#include "checkpoint_ide.h"   /* checkpoint_ide_bind (the durable-store adapter) */

/* ----- Engine link stubs (empty process list; the counter page is driven
 * through capture explicitly, as in test_persistence_e2e). ----- */
pte_t* vmm_get_page_table(vm_space_t* s, uint64_t a, int l, bool c) { (void)s;(void)a;(void)l;(void)c; return 0; }
vm_space_t* vmm_get_current_space(void) { return 0; }
void vmm_flush_tlb_page(uint64_t a) { (void)a; }
uint64_t vmm_get_physical_addr(vm_space_t* s, uint64_t a) { (void)s;(void)a; return 0; }
vm_space_t* vmm_create_address_space(uint32_t pid) { (void)pid; return 0; }
uint64_t vmm_alloc_page(void) { return 0; }
int vmm_map_page(vm_space_t* s, uint64_t v, uint64_t p, uint32_t f) { (void)s;(void)v;(void)p;(void)f; return 0; }
struct process;
int pm_get_process_list(uint32_t* p, uint32_t m, uint32_t* c) { (void)p;(void)m; if (c) *c = 0; return 0; }
struct process* pm_get_process(uint32_t pid) { (void)pid; return 0; }
struct process* process_get_by_pid(int pid) { (void)pid; return 0; }
struct process* process_create(const char* a, const char* b) { (void)a;(void)b; return 0; }
int pm_table_add_process(struct process* p) { (void)p; return 0; }
int scheduler_add_process(struct process* p) { (void)p; return 0; }

/* ----- File-backed "IDE disk": the IDE driver entry points checkpoint_ide.c
 * calls, backed by the image file so contents survive between processes. ----- */
#define DISK_SECTORS 2048
#define IDE_SUCCESS  0
static FILE* g_img;

int ide_read_sectors(void* dev, uint8_t drive, uint64_t lba, uint16_t count, void* buf) {
    (void)dev; (void)drive;
    if (lba + count > DISK_SECTORS) return -1;
    if (fseek(g_img, (long)lba * SNAPSHOT_SECTOR_SIZE, SEEK_SET) != 0) return -1;
    return fread(buf, 1, (size_t)count * SNAPSHOT_SECTOR_SIZE, g_img)
               == (size_t)count * SNAPSHOT_SECTOR_SIZE ? IDE_SUCCESS : -1;
}
int ide_write_sectors(void* dev, uint8_t drive, uint64_t lba, uint16_t count, const void* buf) {
    (void)dev; (void)drive;
    if (lba + count > DISK_SECTORS) return -1;
    if (fseek(g_img, (long)lba * SNAPSHOT_SECTOR_SIZE, SEEK_SET) != 0) return -1;
    if (fwrite(buf, 1, (size_t)count * SNAPSHOT_SECTOR_SIZE, g_img)
            != (size_t)count * SNAPSHOT_SECTOR_SIZE) return -1;
    fflush(g_img); /* push to disk: a "power cut" after this keeps the data */
    return IDE_SUCCESS;
}

/* The counter lives at offset 0 of one persisted page. */
#define COUNTER_PID   1
#define COUNTER_VADDR 0x400000ULL

static int recover_counter(void* ctx, const snapshot_page_record_t* rec) {
    memcpy((uint64_t*)ctx, rec->page_data, sizeof(uint64_t));
    return CHECKPOINT_OK;
}
static uint64_t load_counter(snapshot_store_t* store) {
    checkpoint_init();
    uint64_t counter = 0;
    checkpoint_restore(store, recover_counter, &counter);
    return counter;
}
static int checkpoint_counter(snapshot_store_t* store, uint8_t* page) {
    checkpoint_take();
    pte_t pte = 0x10000ULL | PAGE_PRESENT | PAGE_SNAPSHOT_COW;
    int rc = checkpoint_capture_page(COUNTER_PID, COUNTER_VADDR, page, &pte);
    if (rc != CHECKPOINT_OK) return rc;
    return checkpoint_writeback(store);
}

static FILE* open_disk(const char* path) {
    FILE* f = fopen(path, "r+b");
    if (f) return f;
    f = fopen(path, "w+b");
    if (!f) return 0;
    uint8_t zero[SNAPSHOT_SECTOR_SIZE];
    memset(zero, 0, sizeof(zero));
    for (int i = 0; i < DISK_SECTORS; i++) fwrite(zero, 1, sizeof(zero), f);
    fflush(f);
    return f;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: %s <diskfile> run <N> | verify <V> | show\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    const char* mode = argv[2];

    g_img = open_disk(path);
    if (!g_img) { printf("cannot open %s\n", path); return 2; }

    /* Bind the image as an IDE-backed durable store, exactly as the kernel does
     * at boot via checkpoint_ide_boot_bind (#201). */
    static int ide_token;
    fat_block_device_t dev;
    checkpoint_ide_binding_t binding;
    if (!checkpoint_ide_bind(&dev, &binding, &ide_token, 0 /*master*/, DISK_SECTORS)) {
        printf("IDE bind failed\n"); return 2;
    }

    snapshot_store_t store;
    if (snapshot_store_init(&store, &dev, CHECKPOINT_STORE_BASE_SECTOR,
                            CHECKPOINT_STORE_SLOT_SECTORS) != SNAPSHOT_OK) {
        printf("store init failed\n"); return 2;
    }

    /* A fresh image has no valid checkpoint yet; format it once. */
    snapshot_reader_t probe;
    if (snapshot_store_load(&store, &probe) != SNAPSHOT_OK) {
        if (snapshot_store_format(&store) != SNAPSHOT_OK) { printf("format failed\n"); return 2; }
    }

    uint64_t counter = load_counter(&store); /* resume from the durable IDE store */

    if (strcmp(mode, "run") == 0) {
        long n = argc > 3 ? atol(argv[3]) : 1;
        uint8_t page[SNAPSHOT_PAGE_SIZE];
        printf("[boot] resumed counter = %llu (from the IDE store)\n", (unsigned long long)counter);
        for (long i = 0; i < n; i++) {
            counter++;
            memset(page, 0, sizeof(page));
            memcpy(page, &counter, sizeof(counter));
            if (checkpoint_counter(&store, page) != CHECKPOINT_OK) {
                printf("checkpoint failed at counter=%llu\n", (unsigned long long)counter);
                fclose(g_img); return 1;
            }
            printf("[run ] count = %llu (checkpointed to IDE disk)\n", (unsigned long long)counter);
        }
        fclose(g_img);
        return 0;
    }
    if (strcmp(mode, "verify") == 0) {
        uint64_t want = argc > 3 ? (uint64_t)atol(argv[3]) : 0;
        printf("[boot] resumed counter = %llu (want %llu)\n",
               (unsigned long long)counter, (unsigned long long)want);
        fclose(g_img);
        if (counter == want) { printf("PASSED: resumed from the durable IDE store\n"); return 0; }
        printf("FAILED: counter did not resume as expected\n"); return 1;
    }
    if (strcmp(mode, "show") == 0) {
        printf("counter = %llu\n", (unsigned long long)counter);
        fclose(g_img);
        return 0;
    }
    printf("unknown mode %s\n", mode);
    fclose(g_img);
    return 2;
}
