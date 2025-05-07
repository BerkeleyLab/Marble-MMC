#include "marble_api.h"
#include "phy_mdio.h"
#include <stdio.h>
#include <string.h>

void mdio_phy_print(int verbose)
{
  uint32_t value;
  value = marble_MDIO_read(MDIO_PHY_REG_CU_SP_STAT_1);
  printf("MDIO PHY Status:\r\n");
  if (verbose) printf( "  reg[%2.2x] = %4.4lx\r\n", (unsigned)MDIO_PHY_REG_CU_SP_STAT_1, (long unsigned)value);
  { // Block scope to avoid stack creep
    reg_cu_sp_stat reg;
    reg.val = (unsigned short)value;
    if (reg.bits.copper_link & reg.bits.global_link_status) printf("  Link UP\r\n");
    else printf("  Link DOWN\r\n");
    printf("  Link Speed: ");
    if (reg.bits.speed_duplex_resolved) {
      if (reg.bits.speed == 2) printf("1000 Mbps\r\n");
      else if (reg.bits.speed == 1) printf("100 Mbps\r\n");
      else if (reg.bits.speed == 0) printf("10 Mbps\r\n");
    } else {
      printf("  Auto-Negotiation Unresolved\r\n");
    }
  }
  value = marble_MDIO_read(MDIO_PHY_REG_CU_STAT);
  if (verbose) printf( "  reg[%2.2x] = %4.4lx\r\n", (unsigned)MDIO_PHY_REG_CU_STAT, (long unsigned)value);
  {
    reg_cu_ctrl reg;
    reg.val = (unsigned short)value;
    if (reg.bits.cu_remote_fault) printf("  Remote Fault\r\n");
  }
  if (verbose) {
    uint16_t regs[] = {
      MDIO_PHY_REG_CU_LP_ABL,
      MDIO_PHY_REG_CU_LP_NP,
      MDIO_PHY_REG_EXT_STAT,
      MDIO_PHY_REG_1000BASET_STAT,
      MDIO_PHY_REG_CU_SP_STAT_1
    };
    for (unsigned int n=0; n<(sizeof(regs)/sizeof(uint16_t)); n++) {
      value = marble_MDIO_read(regs[n]);
      printf( "  reg[%2.2x] = %4.4lx\r\n", regs[n], (long unsigned)value);
    }
  }
   /*
   for (uint16_t i=0; i < 4; i++) {
      value = marble_MDIO_read(i);
      printf( "  reg[%2.2x] = %4.4lx\r\n", i, (long unsigned)value);
      // The cast above is a silly hack needed because gcc and arm-gcc disagree
      // on what size requires %lx rather than %x
      // This cast makes both compilers happy
   }
   */
  return;
}

void mdio_phy_reset(void) {
  uint32_t value = marble_MDIO_read(MDIO_PHY_REG_CU_CTRL);
  value |= (1<<15); // Copper Reset
  marble_MDIO_write(MDIO_PHY_REG_CU_CTRL, value);
  return;
}
