bits 64

global GDT_FLUSH

section .text

; VOID GDT_FLUSH(GDT_PTR *Ptr)
; RDI = pointer to GDT_PTR struct
;
; Loads the new GDT and reloads CS via a far return,
; then reloads DS/ES/FS/GS/SS with the kernel data selector (0x10).

GDT_FLUSH:
    lgdt [rdi]

    ; reload CS: push new CS selector + return address, then retfq
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; far return trick to reload CS with selector 0x08
    pop rdi             ; pop return address
    push 0x08           ; push kernel code selector
    push rdi            ; push return address back
    retfq

section .note.GNU-stack noalloc noexec nowrite progbits
