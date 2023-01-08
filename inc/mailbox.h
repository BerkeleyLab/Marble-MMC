#ifndef MAILBOX_H_
#define MAILBOX_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "console.h"
#include "mailbox_def.h"

void mbox_update(bool verbose);
void mbox_peek(void);
uint16_t mbox_get_update_count(void);
void mbox_reset_update_count(void);
void mbox_handle_fmc_mgt_ctl(uint8_t fmc_mgt_cmd);
void mbox_read_page(uint8_t page_no, uint8_t page_sz, uint8_t *page);
void mbox_write_page(uint8_t page_no, uint8_t page_sz, const uint8_t page[]);

int push_fpga_mac_ip(mac_ip_data_t *pmac_ip_data);
//int push_fpga_mac_ip(unsigned char data[10]);

#ifdef __cplusplus
}
#endif

#endif // MAILBOX_H_
