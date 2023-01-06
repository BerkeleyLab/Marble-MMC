#ifndef MAILBOX_H_
#define MAILBOX_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "console.h"
#include "mailbox_def.h"

void mbox_update(bool verbose);
void mbox_peek(void);

int push_fpga_mac_ip(mac_ip_data_t *pmac_ip_data);
//int push_fpga_mac_ip(unsigned char data[10]);

#ifdef __cplusplus
}
#endif

#endif // MAILBOX_H_
