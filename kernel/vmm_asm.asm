; IKOS Virtual Memory Manager - Assembly Support
; Low-level assembly functions for VMM operations

[BITS 64]

section .text

; Get CR2 register (contains page fault address)
global get_cr2
get_cr2:
    mov rax, cr2
    ret

; Get CR3 register (current page table)
global get_cr3
get_cr3:
    mov rax, cr3
    ret

; Set CR3 register (switch page table)
global set_cr3
set_cr3:
    mov cr3, rdi
    ret

; Flush entire TLB
global flush_tlb
flush_tlb:
    mov rax, cr3
    mov cr3, rax
    ret

; Flush single TLB page
global flush_tlb_page
flush_tlb_page:
    invlpg [rdi]
    ret

; Enable paging (set CR0.PG bit)
global enable_paging
enable_paging:
    mov rax, cr0
    or rax, 0x80000000  ; Set PG bit
    mov cr0, rax
    ret

; Disable paging (clear CR0.PG bit)
global disable_paging
disable_paging:
    mov rax, cr0
    and rax, 0x7FFFFFFF ; Clear PG bit
    mov cr0, rax
    ret

; Copy page content
; void copy_page(void* dest, void* src)
global copy_page
copy_page:
    push rcx
    push rsi
    push rdi
    
    mov rcx, 512        ; 4096 bytes / 8 bytes per qword
    cld                 ; Clear direction flag
    rep movsq           ; Copy qwords
    
    pop rdi
    pop rsi
    pop rcx
    ret

; Zero page content
; void zero_page(void* page)
global zero_page
zero_page:
    push rcx
    push rax
    push rdi
    
    xor rax, rax        ; Zero value
    mov rcx, 512        ; 4096 bytes / 8 bytes per qword
    cld                 ; Clear direction flag
    rep stosq           ; Store zero qwords
    
    pop rdi
    pop rax
    pop rcx
    ret

; Memory barrier
global memory_barrier
memory_barrier:
    mfence
    ret

; Check if address is canonical
; bool is_canonical_address(uint64_t addr)
global is_canonical_address
is_canonical_address:
    mov rax, rdi
    shl rax, 16         ; Shift left 16 bits
    sar rax, 16         ; Arithmetic shift right 16 bits
    cmp rax, rdi        ; Compare with original
    sete al             ; Set AL if equal (canonical)
    movzx rax, al       ; Zero-extend to full register
    ret
