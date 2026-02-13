#ifndef VERIFY_H
#define VERIFY_H

#include <stdbool.h>
#include "rem.h"
#include "event_log.h"
#include "firmware_state.h"

/* Internal function declarations */
bool read_token_rem(rem_t rems[REM_COUNT]);
void verify_single_rem(int rem_index, const rem_t* rem1, const rem_t* rem2);

/**
 * @brief Convert byte array to hex string
 *
 * @param bytes Byte array to convert
 * @param len Length of byte array
 * @param hex_str Output hex string (must be at least 2*len+1 bytes)
 */
void bytes_to_hex_string(const uint8_t* bytes, size_t len, char* hex_str);

/**
 * @brief Parse JSON reference file
 *
 * @param filename JSON file path
 * @param ref Firmware reference structure to fill
 * @return true Parsing successful
 * @return false Parsing failed
 */
bool parse_json_file(const char* filename, firmware_reference_t* ref);

/**
 * @brief Free firmware reference structure
 *
 * @param ref Firmware reference structure to free
 */
void free_firmware_reference(firmware_reference_t* ref);

/**
 * @brief Compare hash value and print result
 *
 * @param component_name Name of the component being verified
 * @param ref_hash Reference hash value as hex string
 * @param actual_hash Actual hash value as byte array
 * @param hash_size Size of actual hash in bytes
 * @return true Hash values match
 * @return false Hash values don't match
 */
bool compare_and_print_hash(const char* component_name,
                            const char* ref_hash,
                            const uint8_t* actual_hash,
                            size_t hash_size);

/**
 * @brief Verify firmware state hash values
 *
 * @param json_file JSON file path
 * @param state Firmware state
 * @return true Verification successful
 * @return false Verification failed
 */
bool verify_firmware_state(const char* json_file, const firmware_log_state_t* state);


#endif /* VERIFY_H */