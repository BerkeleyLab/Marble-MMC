/* I need a place for exported function signatures that are
 * only in the sim platform
 */

#ifndef __SIM_API_H
#define __SIM_API_H

#ifdef __cplusplus
extern "C" {
#endif

int sim_spi_init(void);
void init_sim_ltm4673(void);

#ifdef __cplusplus
}
#endif

#endif // __SIM_API_H
