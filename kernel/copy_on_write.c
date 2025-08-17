/* IKOS Copy-on-Write (COW) Implementation
 * Provides efficient memory sharing with copy-on-write semantics
 */

#include "../include/user_space_memory.h"
#include "../include/memory_advanced.h"
#include <string.h>

/* COW page tracking */
static struct {
    uint64_t cow_pages_created;
    uint64_t cow_pages_copied;
    uint64_t cow_faults_handled;
    uint64_t memory_saved;
} cow_stats = {0};

/* COW page structure for tracking shared pages */
typedef struct cow_page {
    struct page*           page;                 /* Physical page */
    atomic_t               refcount;             /* Reference count */
    uint64_t               original_addr;        /* Original virtual address */
    uint32_t               flags;                /* COW flags */
    struct cow_page*       next;                 /* Next in hash chain */
} cow_page_t;

/* Simple hash table for COW page tracking */
#define COW_HASH_SIZE 256
static cow_page_t* cow_hash_table[COW_HASH_SIZE] = {0};
static volatile int cow_hash_lock = 0;

/* COW flags */
#define COW_FLAG_SHARED        0x01              /* Page is shared */
#define COW_FLAG_WRITTEN       0x02              /* Page has been written */
#define COW_FLAG_READONLY      0x04              /* Page is read-only */

/* ========================== COW Hash Table Management ========================== */

static uint32_t cow_hash_function(struct page* page) {
    uint64_t addr = (uint64_t)page;
    return (addr >> 12) % COW_HASH_SIZE;
}

static cow_page_t* cow_find_page(struct page* page) {
    uint32_t hash = cow_hash_function(page);
    cow_page_t* cow_page;
    
    for (cow_page = cow_hash_table[hash]; cow_page; cow_page = cow_page->next) {
        if (cow_page->page == page) {
            return cow_page;
        }
    }
    
    return NULL;
}

static int cow_add_page(struct page* page, uint64_t addr) {
    uint32_t hash = cow_hash_function(page);
    cow_page_t* cow_page;
    
    /* Allocate COW page structure */
    cow_page = kmalloc_new(sizeof(cow_page_t), GFP_KERNEL);
    if (!cow_page) {
        return -USMM_ENOMEM;
    }
    
    /* Initialize COW page */
    cow_page->page = page;
    atomic_set(&cow_page->refcount, 1);
    cow_page->original_addr = addr;
    cow_page->flags = COW_FLAG_SHARED | COW_FLAG_READONLY;
    
    /* Add to hash table */
    cow_page->next = cow_hash_table[hash];
    cow_hash_table[hash] = cow_page;
    
    cow_stats.cow_pages_created++;
    
    return USMM_SUCCESS;
}

static void cow_remove_page(struct page* page) {
    uint32_t hash = cow_hash_function(page);
    cow_page_t* cow_page, *prev;
    
    prev = NULL;
    for (cow_page = cow_hash_table[hash]; cow_page; cow_page = cow_page->next) {
        if (cow_page->page == page) {
            /* Remove from hash table */
            if (prev) {
                prev->next = cow_page->next;
            } else {
                cow_hash_table[hash] = cow_page->next;
            }
            
            /* Free COW page structure */
            kfree_new(cow_page);
            return;
        }
        prev = cow_page;
    }
}

/* ========================== COW Page Management ========================== */

int setup_cow_mapping(vm_area_struct_t* vma) {
    uint64_t addr;
    struct page* page;
    
    if (!vma) {
        return -USMM_EINVAL;
    }
    
    /* Only setup COW for writable private mappings */
    if (!(vma->vm_flags & VM_WRITE) || (vma->vm_flags & VM_SHARED)) {
        return USMM_SUCCESS;
    }
    
    /* Mark VMA as COW */
    vma->vm_flags |= VM_DONTCOPY; /* Don't copy on fork by default */
    
    /* For each page in the VMA, setup COW tracking */
    for (addr = vma->vm_start; addr < vma->vm_end; addr += 4096) {
        /* In real implementation, would get the physical page */
        page = (struct page*)addr; /* Placeholder */
        
        if (page) {
            cow_add_page(page, addr);
        }
    }
    
    return USMM_SUCCESS;
}

void cow_page_dup(struct page* page) {
    cow_page_t* cow_page;
    
    if (!page) {
        return;
    }
    
    cow_page = cow_find_page(page);
    if (cow_page) {
        atomic_inc(&cow_page->refcount);
    }
}

void cow_page_free(struct page* page) {
    cow_page_t* cow_page;
    
    if (!page) {
        return;
    }
    
    cow_page = cow_find_page(page);
    if (cow_page) {
        if (atomic_dec_and_test(&cow_page->refcount)) {
            /* Last reference - remove from tracking */
            cow_remove_page(page);
            
            /* Free the actual page (in real implementation) */
            /* buddy_free_pages(page, 0); */
        }
    }
}

int cow_page_fault(vm_area_struct_t* vma, uint64_t address) {
    struct page* old_page, *new_page;
    cow_page_t* cow_page;
    uint64_t page_addr;
    void* old_data, *new_data;
    
    if (!vma) {
        return -USMM_EINVAL;
    }
    
    /* Check if this is a COW fault */
    if (!(vma->vm_flags & VM_WRITE)) {
        return -USMM_EACCES;
    }
    
    page_addr = address & ~0xFFF;
    
    /* Get the current page */
    old_page = (struct page*)page_addr; /* Placeholder - would look up in page table */
    
    if (!old_page) {
        return -USMM_EFAULT;
    }
    
    /* Check if this page is shared */
    cow_page = cow_find_page(old_page);
    if (!cow_page) {
        /* Not a COW page - regular page fault */
        return -USMM_EFAULT;
    }
    
    /* Check if page is still shared */
    if (atomic_read(&cow_page->refcount) == 1) {
        /* Last reference - can write directly */
        cow_page->flags &= ~COW_FLAG_READONLY;
        cow_page->flags |= COW_FLAG_WRITTEN;
        
        /* Make page writable in page table */
        /* set_page_writable(address); */
        
        return USMM_SUCCESS;
    }
    
    /* Page is shared - need to copy */
    
    /* Allocate new page */
    new_page = (struct page*)kmalloc_new(4096, GFP_KERNEL);
    if (!new_page) {
        return -USMM_ENOMEM;
    }
    
    /* Copy page contents */
    old_data = (void*)old_page; /* Map old page */
    new_data = (void*)new_page; /* Map new page */
    memcpy(new_data, old_data, 4096);
    
    /* Update page table to point to new page */
    /* update_page_table(address, new_page); */
    
    /* Decrease reference count on old page */
    atomic_dec(&cow_page->refcount);
    
    /* Add new page to COW tracking */
    cow_add_page(new_page, address);
    cow_page = cow_find_page(new_page);
    if (cow_page) {
        cow_page->flags &= ~COW_FLAG_READONLY;
        cow_page->flags |= COW_FLAG_WRITTEN;
    }
    
    /* Update statistics */
    cow_stats.cow_faults_handled++;
    cow_stats.cow_pages_copied++;
    cow_stats.memory_saved += 4096; /* Saved memory until copy */
    
    return USMM_SUCCESS;
}

/* ========================== Fork Support ========================== */

int copy_mm(struct process* child, struct process* parent) {
    mm_struct_t* old_mm, *new_mm;
    vm_area_struct_t* vma, *new_vma;
    
    if (!child || !parent || !parent->mm) {
        return -USMM_EINVAL;
    }
    
    old_mm = parent->mm;
    
    /* Copy the entire address space */
    new_mm = mm_copy(old_mm);
    if (!new_mm) {
        return -USMM_ENOMEM;
    }
    
    /* Set up COW for all writable private mappings */
    for (vma = new_mm->mmap; vma; vma = vma->vm_next) {
        if ((vma->vm_flags & (VM_WRITE | VM_SHARED)) == VM_WRITE) {
            /* This is a writable private mapping - set up COW */
            setup_cow_mapping(vma);
            
            /* Also update the parent's corresponding VMA */
            new_vma = find_vma(old_mm, vma->vm_start);
            if (new_vma) {
                setup_cow_mapping(new_vma);
            }
            
            /* Mark pages as read-only in both processes */
            /* mark_pages_readonly(vma->vm_start, vma->vm_end); */
        }
    }
    
    /* Assign to child process */
    child->mm = new_mm;
    
    return USMM_SUCCESS;
}

void exit_mm(struct process* task) {
    mm_struct_t* mm;
    vm_area_struct_t* vma;
    uint64_t addr;
    struct page* page;
    
    if (!task || !task->mm) {
        return;
    }
    
    mm = task->mm;
    
    /* Release COW references for all pages */
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        for (addr = vma->vm_start; addr < vma->vm_end; addr += 4096) {
            /* Get the page at this address */
            page = (struct page*)addr; /* Placeholder */
            
            if (page) {
                cow_page_free(page);
            }
        }
    }
    
    /* Free the address space */
    mm_free(mm);
    task->mm = NULL;
}

/* ========================== Page Fault Handler Integration ========================== */

int handle_mm_fault(mm_struct_t* mm, vm_area_struct_t* vma, uint64_t address, uint32_t flags) {
    if (!mm || !vma) {
        return -USMM_EINVAL;
    }
    
    /* Check if address is within VMA */
    if (address < vma->vm_start || address >= vma->vm_end) {
        return -USMM_EFAULT;
    }
    
    /* Check if this is a write fault on a COW page */
    if (flags & FAULT_FLAG_WRITE) {
        int result = cow_page_fault(vma, address);
        if (result == USMM_SUCCESS) {
            return USMM_SUCCESS;
        }
    }
    
    /* Handle other types of page faults */
    if (vma->vm_ops && vma->vm_ops->fault) {
        return vma->vm_ops->fault(vma, address);
    }
    
    /* Default fault handling */
    if (!(vma->vm_flags & VM_READ)) {
        return -USMM_EACCES;
    }
    
    /* Allocate page for anonymous mapping */
    if (!vma->vm_file) {
        struct page* page = (struct page*)kmalloc_new(4096, GFP_KERNEL);
        if (!page) {
            return -USMM_ENOMEM;
        }
        
        /* Clear the page */
        memset((void*)page, 0, 4096);
        
        /* Map page into address space */
        /* map_page_to_address(address, page); */
        
        /* Update RSS statistics */
        atomic64_inc(&mm->anon_rss);
    }
    
    return USMM_SUCCESS;
}

int handle_page_fault(uint64_t address, uint32_t error_code) {
    struct process* current;
    mm_struct_t* mm;
    vm_area_struct_t* vma;
    uint32_t fault_flags;
    
    /* Get current process */
    current = get_current_process();
    if (!current || !current->mm) {
        return -USMM_EFAULT;
    }
    
    mm = current->mm;
    
    /* Find VMA containing the faulting address */
    vma = find_vma(mm, address);
    if (!vma) {
        return -USMM_EFAULT;
    }
    
    /* Determine fault type */
    fault_flags = 0;
    if (error_code & 0x02) {
        fault_flags |= FAULT_FLAG_WRITE;
    }
    
    /* Handle the fault */
    int result = handle_mm_fault(mm, vma, address, fault_flags);
    
    /* Update statistics */
    if (result == USMM_SUCCESS) {
        global_usmm_stats.page_faults++;
        if (fault_flags & FAULT_FLAG_WRITE) {
            global_usmm_stats.cow_faults++;
        }
    }
    
    return result;
}

/* ========================== Statistics and Monitoring ========================== */

void get_cow_stats(struct {
    uint64_t cow_pages_created;
    uint64_t cow_pages_copied;
    uint64_t cow_faults_handled;
    uint64_t memory_saved;
}* stats) {
    if (stats) {
        stats->cow_pages_created = cow_stats.cow_pages_created;
        stats->cow_pages_copied = cow_stats.cow_pages_copied;
        stats->cow_faults_handled = cow_stats.cow_faults_handled;
        stats->memory_saved = cow_stats.memory_saved;
    }
}

void reset_cow_stats(void) {
    memset(&cow_stats, 0, sizeof(cow_stats));
}

/* ========================== Debugging Support ========================== */

void dump_cow_pages(void) {
    cow_page_t* cow_page;
    int i, total_pages = 0;
    
    for (i = 0; i < COW_HASH_SIZE; i++) {
        for (cow_page = cow_hash_table[i]; cow_page; cow_page = cow_page->next) {
            total_pages++;
            /* In real implementation, would print page details */
        }
    }
    
    /* Print summary */
    /* printf("Total COW pages: %d\n", total_pages); */
}

int validate_cow_consistency(void) {
    cow_page_t* cow_page;
    int i, errors = 0;
    
    for (i = 0; i < COW_HASH_SIZE; i++) {
        for (cow_page = cow_hash_table[i]; cow_page; cow_page = cow_page->next) {
            /* Validate COW page structure */
            if (!cow_page->page) {
                errors++;
            }
            if (atomic_read(&cow_page->refcount) <= 0) {
                errors++;
            }
        }
    }
    
    return errors;
}

/* External function declarations */
extern usmm_stats_t global_usmm_stats;
extern void* kmalloc_new(size_t size, unsigned long flags);
extern void kfree_new(const void* ptr);
extern mm_struct_t* mm_copy(mm_struct_t* oldmm);
extern void mm_free(mm_struct_t* mm);
extern vm_area_struct_t* find_vma(mm_struct_t* mm, uint64_t addr);
extern struct process* get_current_process(void);
