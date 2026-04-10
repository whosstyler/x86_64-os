#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${ROOT_DIR}/build"
KERNEL_DIR="${ROOT_DIR}/kernel"
UEFI_DIR="${ROOT_DIR}/uefi"
SHARED_DIR="${ROOT_DIR}/shared"

EFI_BIN="${OUTPUT_DIR}/bootx64.efi"
KERNEL_ELF="${OUTPUT_DIR}/kernel.elf"
ESP_IMG="${OUTPUT_DIR}/esp.img"
VDI_IMG="${OUTPUT_DIR}/esp.vdi"

CMD="${1:-all}"

PRINT_USAGE() {
    cat <<'EOF'
usage: bash build.sh [loader|kernel|esp|vdi|run|all|clean]

targets:
  loader  build uefi/loader.c into build/bootx64.efi
  kernel  build kernel/ into build/kernel.elf
  esp     create build/esp.img with BOOTX64.EFI and kernel.elf
  vdi     create build/esp.vdi from build/esp.img
  run     full build + start/update virtualbox vm
  all     run loader + kernel + esp + vdi
  clean   remove build artifacts
EOF
}

REQUIRE_TOOL() {
    local tool_name="$1"
    if ! command -v "${tool_name}" >/dev/null 2>&1; then
        echo "missing required tool: ${tool_name}"
        echo "install with: sudo apt install -y nasm gcc mtools dosfstools"
        exit 1
    fi
}

BUILD_LOADER() {
    REQUIRE_TOOL x86_64-w64-mingw32-gcc

    mkdir -p "${OUTPUT_DIR}"

    x86_64-w64-mingw32-gcc \
      -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone \
      -Wall -Wextra -nostdlib \
      -Wl,--subsystem,10 -Wl,-e,EfiMain -Wl,--image-base,0 \
      -o "${EFI_BIN}" "${UEFI_DIR}/loader.c" "${SHARED_DIR}/mem.c"

    echo "built: ${EFI_BIN}"
}

BUILD_KERNEL() {
    REQUIRE_TOOL nasm
    REQUIRE_TOOL gcc

    mkdir -p "${OUTPUT_DIR}/obj"

    # assemble .asm files
    nasm -f elf64 -o "${OUTPUT_DIR}/obj/entry.o"     "${KERNEL_DIR}/arch/entry.asm"
    nasm -f elf64 -o "${OUTPUT_DIR}/obj/gdt_flush.o" "${KERNEL_DIR}/arch/gdt_flush.asm"
    nasm -f elf64 -o "${OUTPUT_DIR}/obj/isr.o"       "${KERNEL_DIR}/arch/isr.asm"
    nasm -f elf64 -o "${OUTPUT_DIR}/obj/switch.o"    "${KERNEL_DIR}/sched/switch.asm"
    nasm -f elf64 -o "${OUTPUT_DIR}/obj/vmx_asm.o"   "${KERNEL_DIR}/hv/vmx_asm.asm"

    local CFLAGS="-ffreestanding -fno-stack-protector -mno-red-zone -nostdlib -Wall -Wextra -mcmodel=large -c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/kernel.o"   "${KERNEL_DIR}/kernel.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/gdt.o"      "${KERNEL_DIR}/arch/gdt.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/idt.o"      "${KERNEL_DIR}/arch/idt.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/pic.o"      "${KERNEL_DIR}/arch/pic.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/security.o" "${KERNEL_DIR}/arch/security.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/timer.o"    "${KERNEL_DIR}/drivers/timer.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/font.o"     "${KERNEL_DIR}/gfx/font.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/pmm.o"      "${KERNEL_DIR}/mm/pmm.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/vmm.o"      "${KERNEL_DIR}/mm/vmm.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/heap.o"     "${KERNEL_DIR}/mm/heap.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/task.o"     "${KERNEL_DIR}/sched/task.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/vmx.o"      "${KERNEL_DIR}/hv/vmx.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/vmexit.o"   "${KERNEL_DIR}/hv/vmexit.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/ept.o"      "${KERNEL_DIR}/hv/ept.c"
    gcc ${CFLAGS} -o "${OUTPUT_DIR}/obj/hv_sec.o"   "${KERNEL_DIR}/hv/security.c"

    ld -T "${KERNEL_DIR}/linker.ld" -nostdlib \
      -o "${KERNEL_ELF}" \
      "${OUTPUT_DIR}/obj/entry.o" \
      "${OUTPUT_DIR}/obj/kernel.o" \
      "${OUTPUT_DIR}/obj/gdt.o" \
      "${OUTPUT_DIR}/obj/gdt_flush.o" \
      "${OUTPUT_DIR}/obj/idt.o" \
      "${OUTPUT_DIR}/obj/isr.o" \
      "${OUTPUT_DIR}/obj/pic.o" \
      "${OUTPUT_DIR}/obj/security.o" \
      "${OUTPUT_DIR}/obj/timer.o" \
      "${OUTPUT_DIR}/obj/font.o" \
      "${OUTPUT_DIR}/obj/pmm.o" \
      "${OUTPUT_DIR}/obj/vmm.o" \
      "${OUTPUT_DIR}/obj/heap.o" \
      "${OUTPUT_DIR}/obj/task.o" \
      "${OUTPUT_DIR}/obj/switch.o" \
      "${OUTPUT_DIR}/obj/vmx.o" \
      "${OUTPUT_DIR}/obj/vmx_asm.o" \
      "${OUTPUT_DIR}/obj/vmexit.o" \
      "${OUTPUT_DIR}/obj/ept.o" \
      "${OUTPUT_DIR}/obj/hv_sec.o"

    echo "built: ${KERNEL_ELF}"
}

BUILD_ESP() {
    REQUIRE_TOOL dd
    REQUIRE_TOOL mkfs.vfat
    REQUIRE_TOOL mmd
    REQUIRE_TOOL mcopy

    if [[ ! -f "${EFI_BIN}" ]]; then
        BUILD_LOADER
    fi
    if [[ ! -f "${KERNEL_ELF}" ]]; then
        BUILD_KERNEL
    fi

    dd if=/dev/zero of="${ESP_IMG}" bs=1M count=64 status=none
    mkfs.vfat "${ESP_IMG}" >/dev/null
    mmd -i "${ESP_IMG}" ::/EFI ::/EFI/BOOT
    mcopy -i "${ESP_IMG}" "${EFI_BIN}" ::/EFI/BOOT/BOOTX64.EFI
    mcopy -i "${ESP_IMG}" "${KERNEL_ELF}" ::/kernel.elf

    echo "built: ${ESP_IMG}"
}

BUILD_VDI() {
    REQUIRE_TOOL VBoxManage

    if [[ ! -f "${ESP_IMG}" ]]; then
        BUILD_ESP
    fi

    rm -f "${VDI_IMG}"
    VBoxManage convertfromraw "${ESP_IMG}" "${VDI_IMG}" --format VDI >/dev/null

    echo "built: ${VDI_IMG}"
}

CLEAN() {
    rm -rf "${OUTPUT_DIR}"
    echo "removed: ${OUTPUT_DIR}"
}

RUN_VM() {
    BUILD_LOADER
    BUILD_KERNEL
    BUILD_ESP
    bash "${ROOT_DIR}/run_vbox.sh" --rebuild-vdi
}

case "${CMD}" in
    loader)
        BUILD_LOADER
        ;;
    kernel)
        BUILD_KERNEL
        ;;
    esp)
        BUILD_ESP
        ;;
    vdi)
        BUILD_VDI
        ;;
    all)
        BUILD_LOADER
        BUILD_KERNEL
        BUILD_ESP
        BUILD_VDI
        ;;
    run)
        RUN_VM
        ;;
    clean)
        CLEAN
        ;;
    *)
        PRINT_USAGE
        exit 1
        ;;
esac
