# kernel

![screenshot](assets/image.png)

Simple x86_64 operating system written from scratch in C and assembly. Boots via UEFI, no Linux kernel involved.

What we got so far:

UEFI bootloader that loads an ELF kernel from the ESP, gets the memory map and framebuffer info from GOP, exits boot services, and jumps to ring 0.

Framebuffer console with an 8x16 bitmap font for early text output. GDT, IDT, PIC remapping, and a PIT timer running at 100hz.

Physical memory manager using a bitmap allocator over the UEFI memory map. Virtual memory manager with 4 level paging, identity mapped first 4GB using 2MB huge pages, and a higher half kernel mirror at 0xFFFF800000000000. Kernel heap allocator (kmalloc/kfree/krealloc) built on top of the PMM.

Preemptive round robin scheduler with kernel threads. Each task gets its own stack, context switch is 14 instructions of assembly saving and restoring callee saved registers plus RSP.

We also did some page table manipulation stuff. Hidden pages where you allocate physical memory and remove all virtual mappings so it's invisible to any scanner. Split TLB demo exploiting separate instruction and data TLBs to make the same virtual address return different physical pages depending on whether you read or execute.

Hardware debug register watchpoints using DR0 through DR7 to trap on memory writes with zero overhead. CR0.WP bypass to write through read only page protection by clearing a single bit in CR0.

Builds with gcc, nasm, and the mingw cross compiler for the UEFI loader. Runs in VirtualBox with EFI enabled.
