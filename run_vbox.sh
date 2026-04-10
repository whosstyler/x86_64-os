#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${ROOT_DIR}/build"
ESP_IMG="${OUTPUT_DIR}/esp.img"
VDI_IMG="${OUTPUT_DIR}/esp.vdi"

VM_NAME="${VM_NAME:-kerneldev}"
VM_BASEDIR="${VM_BASEDIR:-${OUTPUT_DIR}/vbox}"
VM_RAM_MB="${VM_RAM_MB:-512}"
VM_CPUS="${VM_CPUS:-2}"
VM_HEADLESS="${VM_HEADLESS:-0}"

REQUIRE_TOOL() {
    local tool_name="$1"
    if ! command -v "${tool_name}" >/dev/null 2>&1; then
        echo "missing required tool: ${tool_name}"
        exit 1
    fi
}

PRINT_USAGE() {
    cat <<'EOF'
usage: bash run_vbox.sh

flags:
  --rebuild-vdi  recreate build/esp.vdi from build/esp.img before boot

environment variables:
  VM_NAME      virtualbox vm name (default: kerneldev)
  VM_BASEDIR   where vm files are stored (default: build/vbox)
  VM_RAM_MB    vm memory in mb (default: 512)
  VM_CPUS      vm cpu count (default: 2)
  VM_HEADLESS  set 1 to run headless (default: 0)
EOF
}

VM_EXISTS() {
    VBoxManage showvminfo "${VM_NAME}" >/dev/null 2>&1
}

VM_STATE() {
    VBoxManage showvminfo "${VM_NAME}" --machinereadable \
      | awk -F= '/^VMState=/{gsub(/"/, "", $2); print $2}'
}

DETACH_DISK() {
    VBoxManage storageattach "${VM_NAME}" \
      --storagectl "sata" \
      --port 0 \
      --device 0 \
      --type hdd \
      --medium none >/dev/null 2>&1 || true
}

REBUILD_VDI() {
    if [[ ! -f "${ESP_IMG}" ]]; then
        echo "missing ${ESP_IMG}"
        echo "build it first: bash build.sh esp"
        exit 1
    fi

    DETACH_DISK
    VBoxManage closemedium disk "${VDI_IMG}" >/dev/null 2>&1 || true
    rm -f "${VDI_IMG}"
    VBoxManage convertfromraw "${ESP_IMG}" "${VDI_IMG}" --format VDI >/dev/null

    echo "built: ${VDI_IMG}"
}

CREATE_VM() {
    mkdir -p "${VM_BASEDIR}"

    VBoxManage createvm \
      --name "${VM_NAME}" \
      --ostype "Other_64" \
      --basefolder "${VM_BASEDIR}" \
      --register >/dev/null

    VBoxManage modifyvm "${VM_NAME}" \
      --firmware efi \
      --memory "${VM_RAM_MB}" \
      --cpus "${VM_CPUS}" \
      --ioapic on \
      --boot1 disk \
      --boot2 none \
      --boot3 none \
      --boot4 none \
      --graphicscontroller vmsvga >/dev/null

    VBoxManage storagectl "${VM_NAME}" \
      --name "sata" \
      --add sata \
      --controller IntelAhci >/dev/null

    echo "created vm: ${VM_NAME}"
}

ATTACH_DISK() {
    DETACH_DISK

    VBoxManage storageattach "${VM_NAME}" \
      --storagectl "sata" \
      --port 0 \
      --device 0 \
      --type hdd \
      --medium "${VDI_IMG}" >/dev/null

    echo "attached: ${VDI_IMG}"
}

START_VM() {
    local start_mode="gui"

    if [[ "${VM_HEADLESS}" == "1" ]]; then
        start_mode="headless"
    fi

    VBoxManage startvm "${VM_NAME}" --type "${start_mode}" >/dev/null
    echo "started vm: ${VM_NAME} (${start_mode})"
}

main() {
    local rebuild_vdi="0"

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help)
                PRINT_USAGE
                exit 0
                ;;
            --rebuild-vdi)
                rebuild_vdi="1"
                ;;
            *)
                echo "unknown option: $1"
                PRINT_USAGE
                exit 1
                ;;
        esac
        shift
    done

    REQUIRE_TOOL VBoxManage

    if ! VM_EXISTS; then
        CREATE_VM
    fi

    local state
    state="$(VM_STATE)"

    if [[ "${state}" == "running" ]]; then
        echo "vm is already running: ${VM_NAME}"
        echo "power it off before reattaching disk"
        exit 1
    fi

    if [[ "${rebuild_vdi}" == "1" ]]; then
        REBUILD_VDI
    fi

    if [[ ! -f "${VDI_IMG}" ]]; then
        echo "missing ${VDI_IMG}"
        echo "build it first: bash build.sh vdi"
        exit 1
    fi

    ATTACH_DISK
    START_VM
}

main "$@"
