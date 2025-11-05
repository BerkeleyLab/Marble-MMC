#ifndef EEPROM_EMU_H
#define EEPROM_EMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "eeprom.h"

/** @brief Initialize the EEPROM emulation layer
 */
int eeprom_system_init(void);

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

#endif // EEPROM_EMU_H
