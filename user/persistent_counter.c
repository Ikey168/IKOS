/* IKOS demo: the persistent counter (#119).
 *
 * This is the headline demo for IKOS orthogonal persistence. Note what is NOT
 * here: there is no open(), no save(), no load(), no fsync(), no serialization,
 * no "resume from file" logic. The program just counts.
 *
 * Because IKOS checkpoints the entire running system periodically and resumes
 * from the last checkpoint on boot, this counter survives a power cut. Pull the
 * plug at count 4000, plug it back in, and it continues from ~4000 - not 0. The
 * persistence is the operating system's job, not the application's.
 *
 * (Built for the IKOS userland; the same behavior is proven headless on the
 * host by tests/test_persistence_e2e.c, which drives the real checkpoint engine
 * and snapshot store across simulated power cycles.)
 */

#include <stdint.h>

/* Minimal userland I/O hooks provided by the IKOS C runtime. */
extern void print_str(const char* s);
extern void print_u64(uint64_t v);
extern void yield(void); /* cooperatively give up the CPU for a tick */

int main(void) {
    /* A plain variable in the process's address space. Orthogonal persistence
     * checkpoints this memory; on restore the variable already holds its last
     * checkpointed value, so we simply continue counting. */
    uint64_t counter = 0;

    for (;;) {
        counter++;
        print_str("count = ");
        print_u64(counter);
        print_str("\n");
        yield();
    }
    return 0;
}
