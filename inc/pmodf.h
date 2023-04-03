#ifndef PMODF_H_
#define PMODF_H_

/*
 * file: pmodf.h
 * desc: Forwarding bridge between mailbox and pmod
 */

#ifdef __cplusplus
 extern "C" {
#endif

void pmodf_init(void);
void pmodf_handleConfig0(const uint8_t *pdata, int size);
void pmodf_handleData0(const uint8_t *pdata, int size);
void pmodf_returnData0(uint8_t *pdata, int size);

#ifdef __cplusplus
}
#endif

#endif // PMODF_H_
