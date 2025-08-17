/* IKOS Logging & Debugging Service - Debug Symbol Support
 * Provides symbol resolution and stack trace capabilities
 */

#include "../include/logging_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <dlfcn.h>
#include <execinfo.h>

/* ========================== Symbol Table Management ========================== */

static symbol_table_t* g_kernel_symbols = NULL;
static symbol_table_t* g_user_symbols = NULL;

/* ELF parsing helper functions */
static int parse_elf_symbols(const char* filename, symbol_table_t* table) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        return LOG_ERROR_IO;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return LOG_ERROR_IO;
    }
    
    void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        return LOG_ERROR_MEMORY;
    }
    
    /* Parse ELF header */
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)mapped;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        munmap(mapped, st.st_size);
        close(fd);
        return LOG_ERROR_FORMAT;
    }
    
    /* Find section headers */
    Elf64_Shdr* shdr = (Elf64_Shdr*)((char*)mapped + ehdr->e_shoff);
    char* shstrtab = (char*)mapped + shdr[ehdr->e_shstrndx].sh_offset;
    
    /* Find symbol table and string table */
    Elf64_Shdr* symtab_hdr = NULL;
    Elf64_Shdr* strtab_hdr = NULL;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        char* section_name = shstrtab + shdr[i].sh_name;
        
        if (strcmp(section_name, ".symtab") == 0) {
            symtab_hdr = &shdr[i];
        } else if (strcmp(section_name, ".strtab") == 0) {
            strtab_hdr = &shdr[i];
        }
    }
    
    if (!symtab_hdr || !strtab_hdr) {
        munmap(mapped, st.st_size);
        close(fd);
        return LOG_ERROR_NOT_FOUND;
    }
    
    /* Parse symbols */
    Elf64_Sym* symbols = (Elf64_Sym*)((char*)mapped + symtab_hdr->sh_offset);
    char* strtab = (char*)mapped + strtab_hdr->sh_offset;
    uint64_t sym_count = symtab_hdr->sh_size / sizeof(Elf64_Sym);
    
    debug_symbol_t* prev_symbol = NULL;
    table->count = 0;
    
    for (uint64_t i = 0; i < sym_count; i++) {
        Elf64_Sym* sym = &symbols[i];
        
        /* Skip undefined symbols */
        if (sym->st_shndx == SHN_UNDEF) {
            continue;
        }
        
        /* Skip symbols without names */
        if (sym->st_name == 0) {
            continue;
        }
        
        char* sym_name = strtab + sym->st_name;
        
        /* Create debug symbol */
        debug_symbol_t* debug_sym = (debug_symbol_t*)malloc(sizeof(debug_symbol_t));
        if (!debug_sym) {
            continue;
        }
        
        memset(debug_sym, 0, sizeof(debug_symbol_t));
        debug_sym->address = sym->st_value;
        debug_sym->size = sym->st_size;
        
        /* Determine symbol type */
        switch (ELF64_ST_TYPE(sym->st_info)) {
            case STT_FUNC:
                debug_sym->type = SYMBOL_TYPE_FUNCTION;
                break;
            case STT_OBJECT:
                debug_sym->type = SYMBOL_TYPE_VARIABLE;
                break;
            default:
                debug_sym->type = SYMBOL_TYPE_FUNCTION; /* Default */
                break;
        }
        
        /* Copy symbol name */
        debug_sym->name = strdup(sym_name);
        if (!debug_sym->name) {
            free(debug_sym);
            continue;
        }
        
        /* Add to linked list */
        if (prev_symbol) {
            prev_symbol->next = debug_sym;
        } else {
            table->symbols = debug_sym;
        }
        prev_symbol = debug_sym;
        table->count++;
    }
    
    munmap(mapped, st.st_size);
    close(fd);
    
    return LOG_SUCCESS;
}

int debug_load_symbols(const char* file_path, symbol_table_t** table) {
    if (!file_path || !table) {
        return LOG_ERROR_INVALID;
    }
    
    *table = (symbol_table_t*)malloc(sizeof(symbol_table_t));
    if (!*table) {
        return LOG_ERROR_MEMORY;
    }
    
    memset(*table, 0, sizeof(symbol_table_t));
    
    /* Set module name from file path */
    const char* basename = strrchr(file_path, '/');
    if (basename) {
        basename++;
    } else {
        basename = file_path;
    }
    
    (*table)->module_name = strdup(basename);
    if (!(*table)->module_name) {
        free(*table);
        *table = NULL;
        return LOG_ERROR_MEMORY;
    }
    
    /* Parse ELF file */
    int result = parse_elf_symbols(file_path, *table);
    if (result != LOG_SUCCESS) {
        debug_unload_symbols(*table);
        *table = NULL;
        return result;
    }
    
    (*table)->loaded = true;
    
    return LOG_SUCCESS;
}

void debug_unload_symbols(symbol_table_t* table) {
    if (!table) {
        return;
    }
    
    /* Free symbol list */
    debug_symbol_t* symbol = table->symbols;
    while (symbol) {
        debug_symbol_t* next = symbol->next;
        
        free(symbol->name);
        free(symbol->file);
        free(symbol);
        
        symbol = next;
    }
    
    free(table->module_name);
    free(table);
}

debug_symbol_t* debug_find_symbol(symbol_table_t* table, uint64_t address) {
    if (!table || !table->loaded) {
        return NULL;
    }
    
    debug_symbol_t* best_match = NULL;
    uint64_t best_distance = UINT64_MAX;
    
    debug_symbol_t* symbol = table->symbols;
    while (symbol) {
        if (address >= symbol->address && address < symbol->address + symbol->size) {
            /* Exact match within symbol bounds */
            return symbol;
        }
        
        if (address >= symbol->address) {
            uint64_t distance = address - symbol->address;
            if (distance < best_distance) {
                best_distance = distance;
                best_match = symbol;
            }
        }
        
        symbol = symbol->next;
    }
    
    /* Return closest symbol if within reasonable distance (4KB) */
    if (best_match && best_distance <= 4096) {
        return best_match;
    }
    
    return NULL;
}

debug_symbol_t* debug_find_symbol_by_name(symbol_table_t* table, const char* name) {
    if (!table || !table->loaded || !name) {
        return NULL;
    }
    
    debug_symbol_t* symbol = table->symbols;
    while (symbol) {
        if (symbol->name && strcmp(symbol->name, name) == 0) {
            return symbol;
        }
        symbol = symbol->next;
    }
    
    return NULL;
}

/* ========================== Stack Trace Support ========================== */

int debug_capture_stack_trace(stack_trace_t* trace, uint32_t max_frames) {
    if (!trace || max_frames == 0) {
        return LOG_ERROR_INVALID;
    }
    
    memset(trace, 0, sizeof(stack_trace_t));
    trace->max_frames = max_frames;
    
    /* Allocate frame array */
    trace->frames = (stack_frame_t*)malloc(max_frames * sizeof(stack_frame_t));
    if (!trace->frames) {
        return LOG_ERROR_MEMORY;
    }
    
    /* Capture stack using backtrace */
    void* stack_addrs[max_frames];
    int frame_count = backtrace(stack_addrs, max_frames);
    
    if (frame_count <= 0) {
        free(trace->frames);
        trace->frames = NULL;
        return LOG_ERROR_IO;
    }
    
    /* Convert addresses to stack frames */
    for (int i = 0; i < frame_count && i < max_frames; i++) {
        stack_frame_t* frame = &trace->frames[i];
        
        frame->address = (uint64_t)stack_addrs[i];
        frame->return_address = (i < frame_count - 1) ? (uint64_t)stack_addrs[i + 1] : 0;
        
        /* Try to resolve symbol information */
        Dl_info dl_info;
        if (dladdr(stack_addrs[i], &dl_info)) {
            frame->function = dl_info.dli_sname;
            frame->file = dl_info.dli_fname;
            
            if (dl_info.dli_saddr) {
                frame->offset = frame->address - (uint64_t)dl_info.dli_saddr;
            }
        } else {
            frame->function = NULL;
            frame->file = NULL;
            frame->offset = 0;
        }
        
        frame->line = 0; /* Line number resolution requires debug info */
    }
    
    trace->count = frame_count;
    trace->truncated = (frame_count >= max_frames);
    
    return LOG_SUCCESS;
}

void debug_free_stack_trace(stack_trace_t* trace) {
    if (!trace) {
        return;
    }
    
    free(trace->frames);
    memset(trace, 0, sizeof(stack_trace_t));
}

int debug_format_stack_trace(const stack_trace_t* trace, char* buffer, size_t size) {
    if (!trace || !buffer || size == 0) {
        return LOG_ERROR_INVALID;
    }
    
    char* ptr = buffer;
    size_t remaining = size;
    int written;
    
    written = snprintf(ptr, remaining, "Stack trace (%u frames%s):\n",
                      trace->count, trace->truncated ? ", truncated" : "");
    
    if (written < 0 || written >= remaining) {
        return LOG_ERROR_TRUNCATED;
    }
    
    ptr += written;
    remaining -= written;
    
    for (uint32_t i = 0; i < trace->count; i++) {
        const stack_frame_t* frame = &trace->frames[i];
        
        if (frame->function && frame->file) {
            written = snprintf(ptr, remaining, "  #%2u: 0x%016lx in %s (%s+0x%lx)",
                              i, frame->address, frame->function, frame->file, frame->offset);
        } else if (frame->function) {
            written = snprintf(ptr, remaining, "  #%2u: 0x%016lx in %s (+0x%lx)",
                              i, frame->address, frame->function, frame->offset);
        } else {
            written = snprintf(ptr, remaining, "  #%2u: 0x%016lx",
                              i, frame->address);
        }
        
        if (written < 0 || written >= remaining) {
            return LOG_ERROR_TRUNCATED;
        }
        
        ptr += written;
        remaining -= written;
        
        if (frame->line > 0) {
            written = snprintf(ptr, remaining, " at line %u", frame->line);
            if (written > 0 && written < remaining) {
                ptr += written;
                remaining -= written;
            }
        }
        
        written = snprintf(ptr, remaining, "\n");
        if (written < 0 || written >= remaining) {
            return LOG_ERROR_TRUNCATED;
        }
        
        ptr += written;
        remaining -= written;
    }
    
    return LOG_SUCCESS;
}

/* ========================== Symbol Resolution ========================== */

int debug_resolve_address(uint64_t address, char* symbol_name, size_t name_size,
                         char* file_name, size_t file_size, uint32_t* line) {
    if (!symbol_name || name_size == 0) {
        return LOG_ERROR_INVALID;
    }
    
    /* Initialize outputs */
    symbol_name[0] = '\0';
    if (file_name && file_size > 0) {
        file_name[0] = '\0';
    }
    if (line) {
        *line = 0;
    }
    
    /* Try kernel symbols first */
    if (g_kernel_symbols) {
        debug_symbol_t* symbol = debug_find_symbol(g_kernel_symbols, address);
        if (symbol) {
            strncpy(symbol_name, symbol->name, name_size - 1);
            symbol_name[name_size - 1] = '\0';
            
            if (file_name && file_size > 0 && symbol->file) {
                strncpy(file_name, symbol->file, file_size - 1);
                file_name[file_size - 1] = '\0';
            }
            
            if (line) {
                *line = symbol->line;
            }
            
            return LOG_SUCCESS;
        }
    }
    
    /* Try user symbols */
    if (g_user_symbols) {
        debug_symbol_t* symbol = debug_find_symbol(g_user_symbols, address);
        if (symbol) {
            strncpy(symbol_name, symbol->name, name_size - 1);
            symbol_name[name_size - 1] = '\0';
            
            if (file_name && file_size > 0 && symbol->file) {
                strncpy(file_name, symbol->file, file_size - 1);
                file_name[file_size - 1] = '\0';
            }
            
            if (line) {
                *line = symbol->line;
            }
            
            return LOG_SUCCESS;
        }
    }
    
    /* Try dladdr as fallback */
    Dl_info dl_info;
    if (dladdr((void*)address, &dl_info) && dl_info.dli_sname) {
        strncpy(symbol_name, dl_info.dli_sname, name_size - 1);
        symbol_name[name_size - 1] = '\0';
        
        if (file_name && file_size > 0 && dl_info.dli_fname) {
            strncpy(file_name, dl_info.dli_fname, file_size - 1);
            file_name[file_size - 1] = '\0';
        }
        
        return LOG_SUCCESS;
    }
    
    /* No symbol found - format as hex address */
    snprintf(symbol_name, name_size, "0x%016lx", address);
    
    return LOG_ERROR_NOT_FOUND;
}

int debug_addr_to_line(uint64_t address, const char** file, uint32_t* line) {
    if (!file || !line) {
        return LOG_ERROR_INVALID;
    }
    
    *file = NULL;
    *line = 0;
    
    /* Try to find symbol with line information */
    debug_symbol_t* symbol = NULL;
    
    if (g_kernel_symbols) {
        symbol = debug_find_symbol(g_kernel_symbols, address);
    }
    
    if (!symbol && g_user_symbols) {
        symbol = debug_find_symbol(g_user_symbols, address);
    }
    
    if (symbol && symbol->file && symbol->line > 0) {
        *file = symbol->file;
        *line = symbol->line;
        return LOG_SUCCESS;
    }
    
    return LOG_ERROR_NOT_FOUND;
}

/* ========================== Symbol Loading ========================== */

int debug_load_kernel_symbols(void) {
    /* Try common kernel symbol locations */
    const char* kernel_paths[] = {
        "/proc/kallsyms",
        "/boot/System.map",
        "/boot/vmlinux",
        "/vmlinux",
        NULL
    };
    
    for (int i = 0; kernel_paths[i]; i++) {
        if (access(kernel_paths[i], R_OK) == 0) {
            int result = debug_load_symbols(kernel_paths[i], &g_kernel_symbols);
            if (result == LOG_SUCCESS) {
                return LOG_SUCCESS;
            }
        }
    }
    
    return LOG_ERROR_NOT_FOUND;
}

int debug_load_user_symbols(uint32_t process_id) {
    char proc_path[256];
    char exe_path[256];
    ssize_t len;
    
    /* Get executable path from /proc */
    snprintf(proc_path, sizeof(proc_path), "/proc/%u/exe", process_id);
    len = readlink(proc_path, exe_path, sizeof(exe_path) - 1);
    
    if (len > 0) {
        exe_path[len] = '\0';
        
        /* Unload existing user symbols */
        if (g_user_symbols) {
            debug_unload_symbols(g_user_symbols);
            g_user_symbols = NULL;
        }
        
        /* Load new symbols */
        return debug_load_symbols(exe_path, &g_user_symbols);
    }
    
    return LOG_ERROR_NOT_FOUND;
}

/* ========================== Debug Integration ========================== */

/* Enhanced logging with stack trace */
int log_with_stack_trace(log_level_t level, log_facility_t facility, const char* format, ...) {
    /* Create location info */
    log_location_t location = { __FILE__, __func__, __LINE__, 0 };
    
    va_list args;
    va_start(args, format);
    
    /* Log the main message */
    int result = log_message_args(level, facility, format, args);
    va_end(args);
    
    if (result != LOG_SUCCESS) {
        return result;
    }
    
    /* Capture and log stack trace */
    stack_trace_t trace;
    if (debug_capture_stack_trace(&trace, 16) == LOG_SUCCESS) {
        char stack_buffer[2048];
        if (debug_format_stack_trace(&trace, stack_buffer, sizeof(stack_buffer)) == LOG_SUCCESS) {
            log_message_ext(level, facility, LOG_FLAG_STACKTRACE, &location, "%s", stack_buffer);
        }
        debug_free_stack_trace(&trace);
    }
    
    return LOG_SUCCESS;
}

/* Memory debugging helpers */
void debug_log_memory_usage(void) {
    char buffer[512];
    FILE* status = fopen("/proc/self/status", "r");
    
    if (status) {
        while (fgets(buffer, sizeof(buffer), status)) {
            if (strncmp(buffer, "VmSize:", 7) == 0 ||
                strncmp(buffer, "VmRSS:", 6) == 0 ||
                strncmp(buffer, "VmPeak:", 7) == 0) {
                
                /* Remove newline */
                char* newline = strchr(buffer, '\n');
                if (newline) *newline = '\0';
                
                log_debug("Memory: %s", buffer);
            }
        }
        fclose(status);
    }
}

/* Performance debugging */
static uint64_t debug_start_time = 0;

void debug_start_timer(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    debug_start_time = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

uint64_t debug_end_timer(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t end_time = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    return end_time - debug_start_time;
}

void debug_log_timing(const char* operation, uint64_t nanoseconds) {
    if (nanoseconds < 1000) {
        log_debug("Timing: %s took %lu ns", operation, nanoseconds);
    } else if (nanoseconds < 1000000) {
        log_debug("Timing: %s took %.2f μs", operation, nanoseconds / 1000.0);
    } else if (nanoseconds < 1000000000) {
        log_debug("Timing: %s took %.2f ms", operation, nanoseconds / 1000000.0);
    } else {
        log_debug("Timing: %s took %.2f s", operation, nanoseconds / 1000000000.0);
    }
}
