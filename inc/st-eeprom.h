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
#define FOR_ALL_EETAGS() \
  X(1, boot_mode, raw) \
  X(2, mac_addr, mac) \
  X(3, ip_addr, ip4)

typedef enum {
#define X(N, NAME, TYPE)  ee_ ## NAME = N,
  FOR_ALL_EETAGS()
#undef X
} ee_tags_t;

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
int fmc_ee_read(ee_tag_t tag, ee_val_t val);

/** @brief Write to EEPROM
 * @param tag Tag ID.  in range [1, 0xff] inclusive
 * @param val Write buffer
 * @return 0 on Success, negative errno on error
 */
int fmc_ee_write(ee_tag_t tag, const ee_val_t val);

/*
 * @brief Store IP address in eeprom (flash).
 * @param paddr IP Address. Assumed to be at least 'len' bytes in length.
 * @return 0 on Success, negative errno on error
 */
int eeprom_store_ip(uint8_t *paddr, int len);

int eeprom_read_ip(uint8_t *paddr, int len);

/*
 * @brief Store MAC address in eeprom (flash).
 * @param paddr MAC Address. Assumed to be at least 'len' bytes in length.
 * @return 0 on Success, negative errno on error
 */
int eeprom_store_mac(uint8_t *paddr, int len);

int eeprom_read_mac(uint8_t *paddr, int len);

#ifdef __cplusplus
}
#endif

#endif // ST_EEPROM_H
