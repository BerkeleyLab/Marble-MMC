#ifndef ST_EEPROM_H
#define ST_EEPROM_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t ee_tag_t;
typedef uint8_t ee_val_t[6];

typedef struct {
    uint8_t tag;
    uint8_t val[6];
    uint8_t crc;
} ee_frame;

/* This X-macro enumerates all EEPROM tags, with mappings to
 * a symbolic name, and type code.
 *
 * Type codes:
 *   raw - Between 0 -> 6 space separated hex digits
 *   mac - 6 semi-colon separated hex digits
 *   ip4 - 4 dot separated decimal digits
 */
// X(enumval, name, type, size_in_bytes, default_value)
// 'type' is currently unused
// TODO - consolidate IP_LENGTH and MAC_LENGTH from console.h here
#define FOR_ALL_EETAGS() \
  X(1, boot_mode, raw, 1, {0}) \
  X(2, mac_addr,  mac, 6, {18, 85, 85, 0, 1, 46}) \
  X(3, ip_addr,   ip4, 4, {192, 168, 19, 31}) \
  X(4, fan_speed, raw, 1, {100})

typedef enum {
#define X(N, NAME, TYPE, SIZE, ...)  ee_ ## NAME = N,
  FOR_ALL_EETAGS()
#undef X
} ee_tags_t;

/*
 * int eeprom_store_NAME(const uint8_t *pdata, int len);
 * int eeprom_read_NAME(volatile uint8_t *pdata, int len);
 *    Read and write functions auto-generated for all ee_tag_*
 *    entries.  The "NAME" in the function name is the second
 *    arg passed to 'X' in the definition of FOR_ALL_EETAGS()
 *    above.
 *    pdata is assumed to be at least 'len' bytes in length
 *    Return values come from fmc_ee_read() and fmc_ee_write().
 */
#define X(N, NAME, TYPE, SIZE, ...) \
int eeprom_store_ ## NAME(const uint8_t *pdata, int len); \
int eeprom_read_ ## NAME(volatile uint8_t *pdata, int len);
FOR_ALL_EETAGS()
#undef X

/** @brief Initialize EEPROM interface to Flash memory
 *  @param initFlash Erases 'flash' memory (in simulation mode only)
 *  @returns 0 on Success, or negative errno
 */
int eeprom_init(bool initFlash);

/** @brief Reformat EEPROM to erase all settings.
 *  @returns 0 on Success, or negative errno
 */
int fmc_ee_reset(void);

/** @brief Read from EEPROM
 * @param tag Tag ID.  in range [0, 0xff] inclusive
 * @param val Read buffer
 * @return 0 on Success, -ENOENT if not found, other negative errno on error
 */
int fmc_ee_read(ee_tags_t tag, ee_val_t val);

/** @brief Write to EEPROM
 * @param tag Tag ID.  in range [1, 0xff] inclusive
 * @param val Write buffer
 * @return 0 on Success, negative errno on error
 */
int fmc_ee_write(ee_tags_t tag, const ee_val_t val);

#ifdef __cplusplus
}
#endif

#endif // ST_EEPROM_H
