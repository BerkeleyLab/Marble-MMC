#include "marble_api.h"
#include <stdio.h>
#include <string.h>

void phy_print(void)
{
   uint16_t value;
   char p_buf[40];

   for (uint8_t i=0; i < 4; i++) {
      value = marble_MDIO_read(i);

      snprintf(p_buf, 40, "  reg[%2.2x] = %4.4x\r\n", i, value);
      marble_UART_send(p_buf, strlen(p_buf));
   }
}
