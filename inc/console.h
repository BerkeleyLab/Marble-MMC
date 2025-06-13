/*
 * File: console.h
 * Desc: Encapsulate console (UART) interaction API
 *       Line-based comms (not char-based).
 *       Non-blocking if possible.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * The new UART console paradigm will be the following:
 *  * In UART recv ISR:
 *  *   While FIFO not full, Shift received bytes in
 *  *   If any byte is '\n', set msgReady flag
 *  * In main loop:
 *  *   If msgReady, Shift message out of FIFO
 *  *   handleMsg(msg);
 */

#define CONSOLE_MAX_MESSAGE_LENGTH          (128)
#define PRINT_NA() printf("Function not available on this board.\r\n")

#define MAC_LENGTH    (6)
#define IP_LENGTH     (4)

typedef struct {
  uint8_t ip[IP_LENGTH];
  uint8_t mac[MAC_LENGTH];
} mac_ip_data_t;

int console_init(void);
int console_service(void);
void console_pend_msg(void);
int console_push_fpga_mac_ip(void);
void console_print_mac_ip(void);
void console_pend_FPGA_enable(void);

void set_last_ip(const uint8_t *ip);
uint8_t *get_last_ip(void);
void set_last_mac(const uint8_t *mac);
uint8_t *get_last_mac(void);

void CONSOLE_USART_ISR(void);

#ifdef __cplusplus
}
#endif

#endif // __CONSOLE_H
