#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

int hex_to_bytes(unsigned char *in, size_t in_len, unsigned char *out, size_t *out_len);
int download_cert_pem(const char *prefix, const char *filename, const char *url);
int file_exists(const char *prefix, const char *filename);
int read_file_data(const char *file_name, unsigned char **file_data, size_t *file_size);
uint8_t* read_file_data_binary(const char* filename, size_t* out_size);
char* read_text_file(const char* filename, size_t* out_size);
int save_file_data(const char *file_name, unsigned char *file_data, size_t file_size);

#endif /* UTILS_H */
