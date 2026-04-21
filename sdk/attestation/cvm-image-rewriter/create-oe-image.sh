#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

TMP_MOUNT_PATH="/tmp/vm_mount"
MEASURE_IMAGE=false
HASH_ALG="sha256"

ok() {
    echo -e "\e[1;32mSUCCESS: $*\e[0;0m"
}

error() {
    echo -e "\e[1;31mERROR: $*\e[0;0m"
    exit 1
}

warn() {
    echo -e "\e[1;33mWARN: $*\e[0;0m"
}

info() {
    echo -e "\e[0;33mINFO: $*\e[0;0m"
}

check_tool() {
    [[ "$(command -v $1)" ]] || { error "$1 is not installed" 1>&2 ; }
}

usage() {
    cat <<EOM
Usage: $(basename "$0") [OPTION]...
  -h                        Show this help
  -i <input image>          Specify input qcow2 image for measurement
  -a <algorithm>            Specify hash algorithm (sha256 or sha512), default: sha256
EOM
}

process_args() {
    while getopts "v:i:o:s:u:p:fchka:" option; do
        case "$option" in
        i) INPUT_IMAGE=$(realpath "$OPTARG") ;;
        a) 
            if [[ "$OPTARG" != "sha256" && "$OPTARG" != "sha512" ]]; then
                error "Invalid hash algorithm: $OPTARG. Only sha256 and sha512 are supported."
            fi
            HASH_ALG="$OPTARG" ;;
        h)
            usage
            exit 0
            ;;
        *)
            echo "Invalid option '-${OPTARG}'"
            usage
            exit 1
            ;;
        esac
    done

    if [[ -n "${INPUT_IMAGE}" ]]; then
        MEASURE_IMAGE=true
    fi
}

measure_guest_image() {
    local target_image=${1}

    info "Starting measurement process for: ${target_image}"
    info "Using hash algorithm: ${HASH_ALG}"
    guestunmount ${TMP_MOUNT_PATH} 2>/dev/null || true
    gcc measure_pe.c -o MeasurePe -lcrypto -DHASH_ALG=${HASH_ALG}
    mkdir -p ${TMP_MOUNT_PATH}

    guestmount -a ${target_image} -i ${TMP_MOUNT_PATH} || error "Failed to mount the VM image."

    # measure grub
    BOOT_EFI_PATH="${TMP_MOUNT_PATH}/boot/EFI/BOOT/BOOTAA64.EFI"
    [[ -f "${BOOT_EFI_PATH}" ]] || error "BOOTAA64.EFI not found"
    sha_grub=$(./MeasurePe "${BOOT_EFI_PATH}" | grep -oE 'SHA-[0-9]+ = [0-9a-f]+' | awk -F" = " '{print $2}')

    # measure grub.cfg
    GRUB_CFG_PATH="${TMP_MOUNT_PATH}/boot/efi/EFI/openEuler/grub.cfg"
    [[ -f "${GRUB_CFG_PATH}" ]] || error "grub.cfg not found"
    sha_grub_cfg=$(${HASH_ALG}sum "${GRUB_CFG_PATH}" | awk '{print $1}')

    mkdir -p "${TMP_MOUNT_PATH}/tmp/kernel_uncompressed"

    # initialize json
    JSON_TEMPLATE='{"grub": "%s", "grub.cfg": "%s", "kernels": [], "hash_alg": "%s"}'
    printf "${JSON_TEMPLATE}" "${sha_grub}" "${sha_grub_cfg}" "${HASH_ALG}" > image_reference_measurement.json

    find "${TMP_MOUNT_PATH}/boot" -name 'vmlinuz-*' -not -name 'vmlinuz-*rescue*' | while read kernel_path; do
        kernel_file=$(basename "${kernel_path}")
        version="${kernel_file#vmlinuz-}"

        # measure kernel
        uncompressed_path="${TMP_MOUNT_PATH}/tmp/kernel_uncompressed/${kernel_file}"
        gunzip -c "${kernel_path}" > "${uncompressed_path}" 2>/dev/null
        if [ $? -ne 0 ]; then
            warn "Failed to uncompress kernel: ${kernel_file}"
            continue
        fi
        kernel_hash=$(${HASH_ALG}sum "${uncompressed_path}" | awk '{print $1}')
        rm -f "${uncompressed_path}"

        # measure initramfs
        initramfs_path="${TMP_MOUNT_PATH}/boot/initramfs-${version}.img"
        if [ -f "${initramfs_path}" ]; then
            initramfs_hash=$(${HASH_ALG}sum "${initramfs_path}" | awk '{print $1}')
        else
            warn "Missing initramfs for kernel: ${version}"
            initramfs_hash="NOT_FOUND"
        fi

        # update json
        jq --arg version "${version}" \
           --arg kernel "${kernel_hash}" \
           --arg initramfs "${initramfs_hash}" \
           '.kernels += [{
               "version": $version,
               "kernel": $kernel,
               "initramfs": $initramfs
           }]' \
           image_reference_measurement.json > tmp.json && mv tmp.json image_reference_measurement.json
    done

    rm -rf "${TMP_MOUNT_PATH}/tmp/kernel_uncompressed"
    guestunmount ${TMP_MOUNT_PATH}
}

process_args "$@"

if [[ ${MEASURE_IMAGE} == true ]]; then
    measure_guest_image "${INPUT_IMAGE}"
    ok "The measurement process is done"
fi
