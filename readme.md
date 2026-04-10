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

On top of all that we built a security focused hypervisor using Intel VT-x. The kernel launches itself into VMX non-root mode (blue pill style subversion) so the hypervisor runs transparently underneath. It traps CR0 and CR4 writes via VMCS guest-host masks to prevent the kernel from disabling write protection, SMEP, or SMAP. MSR bitmap is configured to intercept writes to LSTAR, IA32_DEBUGCTL, and SYSENTER_EIP so critical system entry points can't be silently redirected.

Hypercall interface using VMCALL with dispatch on RAX for authorized kernel to hypervisor communication. LSTAR can be redirected through the hypercall API but direct WRMSR attempts are denied. Monitor trap flag support for invisible single stepping of guest instructions when the hardware supports it.

VMCS integrity verification runs on every single VM exit, checking HOST_RIP, HOST_RSP, HOST_CR3, MSR bitmap address, CR masks, and execution controls against a known good snapshot. If any field is tampered with the hypervisor halts immediately rather than resuming a compromised guest. The VMXON region, VMCS, and MSR bitmap physical pages are unmapped from the guest virtual address space after launch so ring 0 code can't write to them through the identity mapping.

Anti debug enforcement through the exception bitmap. When enabled the hypervisor traps #DB and #BP exceptions at the VMX level before they reach the guest IDT. Hardware breakpoints are silently cleared and INT3 software breakpoints are swallowed. An attacker can set debug registers but the exceptions never fire from the guest perspective.

EPT (extended page tables) is implemented with identity mapped 2MB huge pages for the first 4GB but currently disabled under VirtualBox due to limitations in its nested VMX emulation. Untested on bare metal and KVM.

Builds with gcc, nasm, and the mingw cross compiler for the UEFI loader. Runs in VirtualBox with EFI and nested VT-x enabled.
