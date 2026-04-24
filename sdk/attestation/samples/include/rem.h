#ifndef REM_H
#define REM_H

#include <stdint.h>
#include <stdbool.h>

#define REM_COUNT 4
#define REM_LENGTH_BYTES 32
#define REM_64_LENGTH_BYTES 64
#define REM_MAX_LENGTH_BYTES    REM_64_LENGTH_BYTES

typedef struct {
    uint8_t data[REM_MAX_LENGTH_BYTES];
} rem_t;

/* REM operation function */
bool rem_init(rem_t* rem);
bool rem_compare(const rem_t* rem1, const rem_t* rem2);
void rem_dump(const rem_t* rem);

#endif /* REM_H */