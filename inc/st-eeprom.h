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

// ==================== Non-Volatile Parameter Management =====================
/* Usage Instructions:
 *    NOTE: Don't touch any of the X-macros unless you have a good reason.
 *    To add a new item to non-volatile memory, simply add it to the macro
 *    definition FOR_ALL_EETAGS() below.  Each entry should be:
 *      X(enumval, name, type, size, default)
 *    Where:
 *      enumval = integer (must be unique! Just increment the last number)
 *      name = name of tag (must be unique!)
 *      type = (currently unused; came from MDS's RTEMS version)
 *      size = size of datum in bytes (must be <= 6).
 *      default = default value as array literal
 *
 *    The expansion of the X-macro here and in st-eeprom.c creates the support
 *    code needed to automatically handle read/store of the variable to/from
 *    non-volatile memory.  At startup, it will automatically search for each
 *    item in flash.  If not found, it will store the value provided as "default"
 *    in the declaration (entry in FOR_ALL_EETAGS).
 *
 *    Two functions are automatically generated for reading/storing.  For a
 *    tag called "my_tag", the signatures would be:
 *      int eeprom_store_my_tag(const uint8_t *pdata, int len);
 *      int eeprom_read_my_tag(volatile uint8_t *pdata, int len);
 *
 *    Their function is fairly self-explanatory.  Note: the explicit 'len'
 *    parameter is included mostly to prevent usage errors.  Fundamentally the
 *    length is intrinsically tied to the tag type (as defined in FOR_ALL_EETAGS)
 *    so we could just assume 'pdata' has size >= that defined for the tag, but
 *    this could lead to less obvious runtime errors.  Requiring the length be
 *    passed at call time could still result in a runtime error but the source
 *    of the problem would be more obvious.
 *
 * Type codes (currently unused - came from MDS's RTEMS version):
 *   raw - Between 0 -> 6 space separated hex digits
 *   mac - 6 semi-colon separated hex digits
 *   ip4 - 4 dot separated decimal digits
 */

// TODO - consolidate IP_LENGTH and MAC_LENGTH from console.h here
#define FOR_ALL_EETAGS() \
  X(1, boot_mode, raw, 1, {0}) \
  X(2, mac_addr,  mac, 6, {18, 85, 85, 0, 1, 46}) \
  X(3, ip_addr,   ip4, 4, {192, 168, 19, 31}) \
  X(4, fan_speed, raw, 1, {102}) \
  X(5, overtemp,  raw, 1, {85}) \
  X(6, mgt_mux,   raw, 1, {0}) \
  X(7, fsynth,    raw, 6, {0, 0, 0, 0, 0, 0})

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
