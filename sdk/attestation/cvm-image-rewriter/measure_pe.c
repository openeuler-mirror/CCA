/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * virtCCA_sdk is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

/* Enumeration of EFI_STATUS */
#define EFI_SUCCESS 0
#define EFI_UNSUPPORTED 2
#define EFI_OUT_OF_RESOURCES 9

#define EFI_ERROR(x) ((x) != EFI_SUCCESS)

typedef size_t uintn_t;

/* Define macros to build data structure signatures from characters. */
#define SIGNATURE_16(A, B) (((A) | ((B) << 8)))
#define SIGNATURE_32(A, B, C, D) (SIGNATURE_16(A, B) | (SIGNATURE_16(C, D) << 16))

#define SHA256_DIGEST_SIZE 32
#define MIN_ARGC 2

/* EXE file formats */
#define EFI_IMAGE_DOS_SIGNATURE SIGNATURE_16('M', 'Z')
#define EFI_IMAGE_NT_SIGNATURE SIGNATURE_32('P', 'E', '\0', '\0')

#define EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b
#define EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES 16
#define EFI_IMAGE_DIRECTORY_ENTRY_SECURITY 4

/* Header Data Directories. */
typedef struct {
    uint32_t virtual_address;
    uint32_t size;
} efi_image_data_directory;

/*
 * PE images can start with an optional DOS header, so if an image is run
 * under DOS it can print an error message.
 */
typedef struct {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
} efi_image_dos_header;

/* COFF File Header (Object and Image). */
typedef struct {
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
} efi_image_file_header;

/* Optional Header Standard Fields for PE32. */
typedef struct {
    /* Standard fields */
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;
    uint32_t base_of_data;

    /* Optional Header Windows-Specific Fields. */
    uint32_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_operating_system_version;
    uint16_t minor_operating_system_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version_value;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint32_t size_of_stack_reserve;
    uint32_t size_of_stack_commit;
    uint32_t size_of_heap_reserve;
    uint32_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rva_and_sizes;
    efi_image_data_directory data_directory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} efi_image_optional_header32;

/* Optional Header Standard Fields for PE32+. */
typedef struct {
    /* Standard fields */
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;

    /* Optional Header Windows-Specific Fields. */
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_operating_system_version;
    uint16_t minor_operating_system_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version_value;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint64_t size_of_stack_reserve;
    uint64_t size_of_stack_commit;
    uint64_t size_of_heap_reserve;
    uint64_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rva_and_sizes;
    efi_image_data_directory data_directory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} efi_image_optional_header64;

typedef struct {
    uint32_t signature;
    efi_image_file_header file_header;
    efi_image_optional_header32 optional_header;
} efi_image_nt_headers32;

typedef struct {
    uint32_t signature;
    efi_image_file_header file_header;
    efi_image_optional_header64 optional_header;
} efi_image_nt_headers64;

#define EFI_IMAGE_SIZEOF_SHORT_NAME 8

/* Section Table. This table immediately follows the optional header. */
typedef struct {
    uint8_t name[EFI_IMAGE_SIZEOF_SHORT_NAME];
    union {
        uint32_t physical_address;
        uint32_t virtual_size;
    } misc;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_linenumbers;
    uint16_t number_of_relocations;
    uint16_t number_of_linenumbers;
    uint32_t characteristics;
} efi_image_section_header;


/* Union of PE32, PE32+ headers. */
typedef union {
    efi_image_nt_headers32 pe32;
    efi_image_nt_headers64 pe32plus;
} efi_image_optional_header_union;

typedef union {
    efi_image_nt_headers32 *pe32;
    efi_image_nt_headers64 *pe32plus;
    efi_image_optional_header_union *union_hdr;
} efi_image_optional_header_ptr_union;

typedef struct {
    unsigned char sha256[32];
} tpml_digest_values_t;

typedef struct {
    EVP_MD_CTX *md_ctx;
} my_hash_context_t;

typedef my_hash_context_t *hash_handle_t;

/* Start hash sequence. */
static uint64_t hash_start(hash_handle_t *hash_handle_out)
{
    if (hash_handle_out == NULL) {
        return EFI_UNSUPPORTED;
    }

    my_hash_context_t *context = (my_hash_context_t *)malloc(sizeof(my_hash_context_t));
    if (context == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    context->md_ctx = EVP_MD_CTX_new();
    if (!context->md_ctx) {
        free(context);
        return EFI_OUT_OF_RESOURCES;
    }

    if (!EVP_DigestInit_ex(context->md_ctx, EVP_sha256(), NULL)) {
        EVP_MD_CTX_free(context->md_ctx);
        free(context);
        return EFI_UNSUPPORTED;
    }

    *hash_handle_out = context;
    return EFI_SUCCESS;
}

/* Update hash sequence data. */
static uint64_t hash_update(hash_handle_t hash_handle, const uint8_t *data, size_t data_size)
{
    if (hash_handle == NULL || data == NULL) {
        return EFI_UNSUPPORTED;
    }

    if (!EVP_DigestUpdate(hash_handle->md_ctx, data, data_size)) {
        return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
}

/* Hash sequence complete and extend to PCR. */
static uint64_t hash_complete_and_extend(hash_handle_t hash_handle, const uint8_t *event_data, size_t event_size,
                                         tpml_digest_values_t *digest_list)
{
    if (hash_handle == NULL || digest_list == NULL) {
        return EFI_UNSUPPORTED;
    }

    unsigned char final_digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;

    if (!EVP_DigestFinal_ex(hash_handle->md_ctx, final_digest, &digest_len)) {
        EVP_MD_CTX_free(hash_handle->md_ctx);
        free(hash_handle);
        return EFI_UNSUPPORTED;
    }

    if (digest_len != SHA256_DIGEST_LENGTH) {
        EVP_MD_CTX_free(hash_handle->md_ctx);
        free(hash_handle);
        return EFI_UNSUPPORTED;
    }

    memcpy(digest_list->sha256, final_digest, SHA256_DIGEST_LENGTH);

    EVP_MD_CTX_free(hash_handle->md_ctx);
    free(hash_handle);

    return EFI_SUCCESS;
}

/* parse DOS/PE headers */
static uint64_t parse_pe_headers(uint64_t image_address,
                                 uint32_t *pe_coff_offset,
                                 efi_image_optional_header_ptr_union *hdr)
{
    efi_image_dos_header *dos_hdr = (efi_image_dos_header *)image_address;
    *pe_coff_offset = 0;

    if (dos_hdr->e_magic == EFI_IMAGE_DOS_SIGNATURE) {
        *pe_coff_offset = dos_hdr->e_lfanew;
    }

    hdr->pe32 = (efi_image_nt_headers32 *)((uint8_t *)image_address + *pe_coff_offset);
    if (hdr->pe32->signature != EFI_IMAGE_NT_SIGNATURE) {
        return EFI_UNSUPPORTED;
    }
    return EFI_SUCCESS;
}

/* Hash the image header from its base to beginning of the image checksum. */
static uint64_t hash_optional_header_part(hash_handle_t hash_handle,
                                          efi_image_optional_header_ptr_union *hdr,
                                          uint64_t image_address,
                                          size_t *sum_hashed)
{
    uint8_t *hash_base;
    size_t hash_size;
    uint32_t number_of_rva_and_sizes;
    bool is_pe32 = (hdr->pe32->optional_header.magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC);

    if (is_pe32) {
        /* Use PE32 offset */
        number_of_rva_and_sizes = hdr->pe32->optional_header.number_of_rva_and_sizes;
        hash_base = (uint8_t *)image_address;
        hash_size = ((uintptr_t)&hdr->pe32->optional_header.checksum) - ((uintptr_t)hash_base);
    } else {
        /* Use pe32+ offset */
        number_of_rva_and_sizes = hdr->pe32plus->optional_header.number_of_rva_and_sizes;
        hash_base = (uint8_t *)image_address;
        hash_size = ((uintptr_t)&hdr->pe32plus->optional_header.checksum) - ((uintptr_t)hash_base);
    }

    uint64_t status = hash_update(hash_handle, hash_base, hash_size);
    if (EFI_ERROR(status))
        return status;

    /* Skip over the image checksum */
    if (number_of_rva_and_sizes <= EFI_IMAGE_DIRECTORY_ENTRY_SECURITY) {
        uint8_t *post_checksum = (uint8_t *)&hdr->pe32->optional_header.checksum + sizeof(uint32_t);
        size_t header_size =
            is_pe32 ? hdr->pe32->optional_header.size_of_headers : hdr->pe32plus->optional_header.size_of_headers;
        hash_size = header_size - (post_checksum - (uint8_t *)image_address);

        if (hash_size > 0) {
            status = hash_update(hash_handle, post_checksum, hash_size);
        }
    } else {
        uint8_t *post_checksum = (uint8_t *)&hdr->pe32->optional_header.checksum + sizeof(uint32_t);
        size_t security_offset = is_pe32 ?
            (uintptr_t)&hdr->pe32->optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY] :
            (uintptr_t)&hdr->pe32plus->optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY];

        hash_size = security_offset - (uintptr_t)post_checksum;
        if (hash_size > 0) {
            status = hash_update(hash_handle, post_checksum, hash_size);
            if (EFI_ERROR(status))
                return status;
        }

        uint8_t *post_security = (uint8_t *)(security_offset + sizeof(efi_image_data_directory));
        size_t header_size =
            is_pe32 ? hdr->pe32->optional_header.size_of_headers : hdr->pe32plus->optional_header.size_of_headers;
        hash_size = header_size - (post_security - (uint8_t *)image_address);

        if (hash_size > 0) {
            status = hash_update(hash_handle, post_security, hash_size);
        }
    }

    *sum_hashed = is_pe32 ? hdr->pe32->optional_header.size_of_headers : hdr->pe32plus->optional_header.size_of_headers;
    return status;
}

static efi_image_section_header *copy_and_sort_sections(uint64_t image_address,
                                                        uint32_t pe_coff_offset,
                                                        efi_image_nt_headers32 *nt_header,
                                                        uint64_t *status)
{
    size_t num_sections = nt_header->file_header.number_of_sections;
    efi_image_section_header *sections = calloc(num_sections, sizeof(efi_image_section_header));
    if (!sections) {
        *status = EFI_OUT_OF_RESOURCES;
        return NULL;
    }

    uint8_t *section_base = (uint8_t *)image_address + pe_coff_offset + sizeof(uint32_t) + // Signature
        sizeof(efi_image_file_header) + nt_header->file_header.size_of_optional_header;

    memcpy(sections, section_base, num_sections * sizeof(efi_image_section_header));

    for (size_t i = 1; i < num_sections; i++) {
        efi_image_section_header temp = sections[i];
        size_t j = i;
        while (j > 0 && sections[j - 1].pointer_to_raw_data > temp.pointer_to_raw_data) {
            sections[j] = sections[j - 1];
            j--;
        }
        sections[j] = temp;
    }

    *status = EFI_SUCCESS;
    return sections;
}

static uint64_t hash_section_data(hash_handle_t hash_handle,
                                  efi_image_section_header *sections,
                                  size_t num_sections,
                                  uint64_t image_address, size_t *sum_hashed)
{
    for (size_t i = 0; i < num_sections; i++) {
        if (sections[i].size_of_raw_data == 0)
            continue;

        uint8_t *section_base = (uint8_t *)image_address + sections[i].pointer_to_raw_data;
        uint64_t status = hash_update(hash_handle, section_base, sections[i].size_of_raw_data);
        if (EFI_ERROR(status))
            return status;

        *sum_hashed += sections[i].size_of_raw_data;
    }
    return EFI_SUCCESS;
}

static uint64_t process_trailing_data(hash_handle_t hash_handle,
                                      efi_image_optional_header_ptr_union *hdr,
                                      uint64_t image_address,
                                      size_t image_size,
                                      size_t sum_hashed)
{
    uint32_t cert_size = 0;
    bool is_pe32 = (hdr->pe32->optional_header.magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC);
    uint32_t number_of_rva = is_pe32 ? hdr->pe32->optional_header.number_of_rva_and_sizes :
                                       hdr->pe32plus->optional_header.number_of_rva_and_sizes;

    if (number_of_rva > EFI_IMAGE_DIRECTORY_ENTRY_SECURITY) {
        cert_size = is_pe32 ? hdr->pe32->optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].size :
                              hdr->pe32plus->optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].size;
    }

    if (image_size > sum_hashed + cert_size) {
        uint8_t *trailing_base = (uint8_t *)image_address + sum_hashed;
        size_t trailing_size = image_size - sum_hashed - cert_size;
        return hash_update(hash_handle, trailing_base, trailing_size);
    }
    return EFI_SUCCESS;
}

static uint64_t measure_pe_image_and_extend(uint64_t image_address,
                                            size_t image_size,
                                            tpml_digest_values_t *digest_list)
{
    uint64_t status;
    efi_image_optional_header_ptr_union hdr;
    uint32_t pe_coff_offset;
    hash_handle_t hash_handle = NULL;
    efi_image_section_header *sorted_sections = NULL;
    size_t sum_hashed = 0;

    /* 1. parse headers */
    if ((status = parse_pe_headers(image_address, &pe_coff_offset, &hdr)) != EFI_SUCCESS) {
        goto cleanup;
    }

    /* 2. initialize hash */
    if ((status = hash_start(&hash_handle)) != EFI_SUCCESS) {
        goto cleanup;
    }

    /* 3. parse optional headers */
    if ((status = hash_optional_header_part(hash_handle, &hdr, image_address, &sum_hashed)) != EFI_SUCCESS) {
        goto cleanup;
    }

    /* 4. copy and sort sections */
    efi_image_nt_headers32 *nt_header = hdr.pe32;
    sorted_sections = copy_and_sort_sections(image_address, pe_coff_offset, nt_header, &status);
    if (EFI_ERROR(status)) {
        goto cleanup;
    }

    /* 5. hash section data */
    if ((status = hash_section_data(hash_handle, sorted_sections, nt_header->file_header.number_of_sections,
        image_address, &sum_hashed)) != EFI_SUCCESS) {
        goto cleanup;
    }

    /* 6. process trailing data */
    if ((status = process_trailing_data(hash_handle, &hdr, image_address, image_size, sum_hashed)) != EFI_SUCCESS) {
        goto cleanup;
    }

    /* 7. finalize the SHA hash */
    status = hash_complete_and_extend(hash_handle, NULL, 0, digest_list);
    hash_handle = NULL;

cleanup:
    if (sorted_sections)
        free(sorted_sections);
    if (hash_handle)
        hash_complete_and_extend(hash_handle, NULL, 0, NULL);
    return status;
}

int main(int argc, char *argv[])
{
    if (argc < MIN_ARGC) {
        printf("Usage: %s <PE_File>\n", argv[0]);
        return -1;
    }

    const char *filename = argv[1];
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    long filesize = ftell(fp);
    if (filesize <= 0) {
        fclose(fp);
        printf("File size invalid.\n");
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    uint8_t *buffer = (uint8_t *)malloc(filesize);
    if (!buffer) {
        fclose(fp);
        printf("Out of memory.\n");
        return -1;
    }

    if (fread(buffer, 1, filesize, fp) != (size_t)filesize) {
        fclose(fp);
        free(buffer);
        printf("Read file error.\n");
        return -1;
    }
    if (fclose(fp) != 0) {
        free(buffer);
        return -1;
    }
    tpml_digest_values_t digest_list;
    uint64_t status = measure_pe_image_and_extend((uint64_t)(uintptr_t)buffer, (size_t)filesize, &digest_list);
    if (status == EFI_SUCCESS) {
        printf("Measure PE image succeed, SHA-256 = ");
        for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
            printf("%02x", digest_list.sha256[i]);
        }
        printf("\n");
    } else {
        printf("Measure PE image failed, EFI_STATUS = %llu\n", (unsigned long long)status);
    }

    free(buffer);
    return 0;
}