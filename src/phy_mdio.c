#include "marble_api.h"
#include "sim_platform.h"
#include "phy_mdio.h"
#include <stdio.h>
#include <string.h>

void phy_print(void)
{
   uint32_t value;

   for (uint16_t i=0; i < 4; i++) {
      value = marble_MDIO_read(i);
      printf( "  reg[%2.2x] = %4.4lx\r\n", i, (long unsigned)value);
      // The cast above is a silly hack needed because gcc and arm-gcc disagree
      // on what size requires %lx rather than %x
      // This cast makes both compilers happy
   }
}
