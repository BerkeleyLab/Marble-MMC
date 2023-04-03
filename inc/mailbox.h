#ifndef MAILBOX_H_
#define MAILBOX_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "console.h"
#include "mailbox_def.h"

#define MBOX_PRINT_PAGE(npage) do { \
  printf("Page %d\r\n", npage); \
  for (int n = 0; n < MB ## npage ## _SIZE; n++) { \
    printf("0x%x ", page[n]); \
  } \
  printf("\r\n"); \
} while (0);

void mbox_service(void);
uint16_t mbox_get_update_count(void);
void mbox_reset_update_count(void);
void mbox_read_page(uint8_t page_no, uint8_t page_sz, uint8_t *page);
void mbox_write_page(uint8_t page_no, uint8_t page_sz, const uint8_t page[]);
// The below write/read to/from the currently selected page
void mbox_write_entry(uint8_t entry_no, uint8_t data);
void mbox_pend_request(void);
uint8_t mbox_read_entry(uint8_t entry_no);

int push_fpga_mac_ip(mac_ip_data_t *pmac_ip_data);
//int push_fpga_mac_ip(unsigned char data[10]);

#ifdef __cplusplus
}
#endif

#endif // MAILBOX_H_
