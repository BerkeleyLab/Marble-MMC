/*
 * Based in part on (and therefore legally derived from):
 * STM32 HAL Library
 *
 * which is COPYRIGHT(c) 2017 STMicroelectronics
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include "stm32f2xx_hal.h"
#include "marble_api.h"
#include "string.h"
#include "uart_fifo.h"
#include "console.h"
#include "eeprom.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "ltm4673.h"
#include "watchdog.h"
#include "rev.h"
#include "phy_mdio.h"

//#define DEBUG_PRINT
#include "dbg.h"

#define AHBCLK_DIV        (RCC_SYSCLK_DIV1)
#define APB1CLK_DIV       (RCC_HCLK_DIV4)
#define APB2CLK_DIV       (RCC_HCLK_DIV2)

#define AHB_CLK_DIV(CLK_DIV) (CLK_DIV == RCC_SYSCLK_DIV1   ? 1   : CLK_DIV == RCC_SYSCLK_DIV2   ? 2   : \
                              CLK_DIV == RCC_SYSCLK_DIV4   ? 4   : CLK_DIV == RCC_SYSCLK_DIV8   ? 8   : \
                              CLK_DIV == RCC_SYSCLK_DIV16  ? 16  : CLK_DIV == RCC_SYSCLK_DIV64  ? 64  : \
                              CLK_DIV == RCC_SYSCLK_DIV128 ? 128 : CLK_DIV == RCC_SYSCLK_DIV256 ? 256 : 512)

#define APB_CLK_DIV(CLK_DIV) (CLK_DIV == RCC_HCLK_DIV1 ? 1 : CLK_DIV == RCC_HCLK_DIV2 ? 2 : \
                              CLK_DIV == RCC_HCLK_DIV4 ? 4 : CLK_DIV == RCC_HCLK_DIV8 ? 8 : 16)

#define FREQUENCY_AHBCLK     (FREQUENCY_SYSCLK/AHB_CLK_DIV(AHBCLK_DIV))
#define FREQUENCY_APB1CLK    (FREQUENCY_AHBCLK/APB_CLK_DIV(APB1CLK_DIV))
#define FREQUENCY_APB2CLK    (FREQUENCY_AHBCLK/APB_CLK_DIV(APB2CLK_DIV))
#define FREQUENCY_APB1TIM    APB1CLK_DIV == RCC_HCLK_DIV1 ? FREQUENCY_APB1CLK : 2*FREQUENCY_APB1CLK
#define FREQUENCY_APB2TIM    APB2CLK_DIV == RCC_HCLK_DIV1 ? FREQUENCY_APB2CLK : 2*FREQUENCY_APB2CLK

#define XRP_BYPASS_PWRGD
#define XRP_REBOOT_DELAY    (500)

#ifdef NUCLEO
#define SMBA_PIN GPIO_PIN_13
#else
#define SMBA_PIN GPIO_PIN_15
#endif

#define PRINT_POWER_STATE(subs, on) do {\
   char s[3] = {'f', 'f', '\0'}; \
   if (on) {s[0] = 'n'; s[1] = '\0';} \
   printf(subs " Power O%s\r\n", s); } while (0)

static const char *ErrorCodeStrings[ERROR_CODE_COUNT] = { // triggers compile warning if not all enum values are covered
    "ERROR_NONE",
    "ERROR_MARBLE_POWERDOWN: Power failure detected",
    "ERROR_MARBLE_OVERTEMP: Over-temperature detected",
    "ERROR_MARBLE_PMOD: PMOD configuration error",
    "ERROR_EEPROM_FAN: fan speed",
    "ERROR_EEPROM_OVERTEMP: over-temperature threshold",
    "ERROR_EEPROM_UPDATE: failed to store page",
    "ERROR_EEPROM_STORE",
    "ERROR_EEPROM_READ",
    "ERROR_RCC_OSC_CONFIG",
    "ERROR_RCC_CLOCK_CONFIG",
    "ERROR_ETH_MDIO_INIT",
    "ERROR_ETH_MDIO_ID - PHY ID mismatch",
    "ERROR_I2C1_INIT - I2C_FPGA bus init failed",
    "ERROR_I2C3_INIT - I2C_PM bus init failed",
    "ERROR_I2C1_DEINIT - I2C_FPGA bus de-init failed",
    "ERROR_I2C3_DEINIT - I2C_PM bus de-init failed",
    "ERROR_SPI1_INIT",
    "ERROR_UART_CONSOLE_INIT",
    "ERROR_I2C_FPGA_NONE - No error",
    "ERROR_I2C_FPGA_BERR - Bus error",
    "ERROR_I2C_FPGA_ARLO - Arbitration lost",
    "ERROR_I2C_FPGA_AF - No ACK received",
    "ERROR_I2C_FPGA_OVR - Overrun error",
    "ERROR_I2C_FPGA_DMA - DMA transfer error",
    "ERROR_I2C_FPGA_TIMEOUT - Timeout error",
    "ERROR_I2C_FPGA_BUSY - Bus busy",
    "ERROR_I2C_FPGA_HW_BUSY",
    "ERROR_I2C_FPGA_LOCKUP",
    "ERROR_I2C_FPGA_ADN4600",
    "ERROR_I2C_FPGA_UNDEFINED - Undefined I2C FPGA error",
    "ERROR_I2C_PM_NONE - No error",
    "ERROR_I2C_PM_BERR - Bus error",
    "ERROR_I2C_PM_ARLO - Arbitration lost",
    "ERROR_I2C_PM_AF - No ACK received",
    "ERROR_I2C_PM_OVR - Overrun error",
    "ERROR_I2C_PM_DMA - DMA transfer error",
    "ERROR_I2C_PM_TIMEOUT - Timeout error",
    "ERROR_I2C_PM_BUSY - Warning: Bus busy",
    "ERROR_I2C_PM_HW_BUSY",
    "ERROR_I2C_PM_LOCKUP",
    "ERROR_I2C_PM_UNDEFINED - Undefined I2C PM error",
    "ERROR_LTM_VOUT - An output voltage fault or warning has occurred",
    "ERROR_LTM_IOUT - An output current fault or warning has occurred",
    "ERROR_LTM_VIN - An input voltage fault or warning has occurred",
    "ERROR_LTM_MFR - A manufacturer specific fault has occurred",
    "ERROR_LTM_POWERNGD - The PWRGD pin, if enabled, is negated. Power is not good",
    "ERROR_LTM_BUSY - Device busy when PMBus command received",
    "ERROR_LTM_NOPOWER - The unit is not providing power to the output",
    "ERROR_LTM_VOUTOVER - An output overvoltage fault has occurred",
    "ERROR_LTM_IOUTOVER - An output overcurrent fault has occurred",
    "ERROR_LTM_VINUNDER - A VIN undervoltage fault has occurred",
    "ERROR_LTM_OVERTEMP - A temperature fault or warning has occurred",
    "ERROR_LTM_COMM - A communication, memory or logic fault has occurred",
    "ERROR_UNDEFINED - Good luck figuring this one out!"
};

static uint32_t error_counters[ERROR_CODE_COUNT] = {0};
static uint32_t error_last_tick[ERROR_CODE_COUNT] = {0};
static uint8_t error_last_caller_id[ERROR_CODE_COUNT] = {0};
static bool errors_muted = false;
uint8_t previous_error = 0xff;
uint8_t repeating_error = 0xff;
static uint8_t tick_overflow_count = 0;

static uint32_t MMC_SN[3] = {0};
static uint16_t Marble_SN = 0;

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif /* USE_FULL_ASSERT */

ETH_HandleTypeDef heth;
RNG_HandleTypeDef hrng;
HAL_StatusTypeDef rng_init_status;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart_console;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;  // Used for nucleo

static Marble_PCB_Rev_t marble_pcb_rev;
static uint32_t boot_id = 0xDEADBEEF;

#ifdef MARBLE_V2
static reset_cause_t reset_cause = RESET_CAUSE_UNKNOWN;
#endif

static int i2cBusStatus = 0;
static int i2c_error_counter = 0;
static int i2c_pm_alert = 0;
static int _over_temp = 0;
// PWR_GOOD sets the length of the PWRGD glitch filter. Higher values means longer.
#define PWR_GOOD 3
#define PWR_FAIL 0
// Assert this so that the first rising edge of PWR_GOOD doesn't trigger re-init
static int _pwr_state = PWR_GOOD;
static int _pwr_good = 1;

#ifdef XRP_BYPASS_PWRGD
static int _do_reboot = 0;
static int _reboot_time = 0;
#endif

// Moved here from marble_api.h
SSP_PORT SSP_FPGA;
SSP_PORT SSP_PMOD;
I2C_BUS I2C_FPGA;
I2C_BUS I2C_PM;
/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void SystemClock_Config_HSI(void);
static void MX_GPIO_Init(void);
static void MX_ETH_MDIO_Init(void);
static void MX_I2C_BusInit(I2C_HandleTypeDef *hi2c, I2C_BUS *bus, I2C_TypeDef *instance);
static void MX_I2C_BusReInit(I2C_BUS I2C_bus);
static void I2C_ClearBus(I2C_BUS I2C_bus);
static void MX_SPI1_Init(void);
//static void MX_SPI2_Init(void);
static void RNG_Init(void);
//static void MX_USART1_UART_Init(void);
static void CONSOLE_USART_Init(void);
//static void MX_USART2_UART_Init(void);
static void marble_read_pcb_rev(void);
static int marble_MGTMUX_store(void);
static void I2C_PM_smba_handler(void);
static int i2c_hook(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw,
                    int cmd, const uint8_t *data, int len);
static void show_mmc_ID(void);
static void show_PHY_ID(void);
static void get_MMC_SN(void);
static void show_MMC_SN(void);
static void get_Marble_SN(void);
static void print_time(uint32_t total_seconds);
static void print_uptime(void);
static void print_clock_info(void);
static void pmod_timer_interrupt_enable(void);
static void pmod_timer_interrupt_disable(void);
static void pmod_config_direction(uint32_t direction);

#ifdef MARBLE_V2
static reset_cause_t reset_cause_get(void);
static const char * reset_cause_get_name(void);
#endif

void disable_all_IRQs(void) {
   // STM32F2 has 80 interrupt channels (plus the 14 in the ARM Cortex-M)
   NVIC->ICER[0] = 0xffffffff;   // 0-31
   NVIC->ICER[1] = 0xffffffff;   // 32-63
   NVIC->ICER[2] = 0xffffffff;   // 64-80
   return;
}

/* void board_init(void);
 *  Board-related (not MMC-related) initialization
 */
void board_init(void) {
  // Initialize subsystems
  I2C_PM_init();

  // Enable clock crosspoint, checking power good in the process
  if (mgtclk_xpoint_en() == 0) {

    LM75_Init();

    // Some non-volatile params go straight to external hardware
    // (i.e. fan speed, overtemp threshold)
    //system_apply_params();

    // This needs to wait for system parameters and external +3V3 power available
    system_off_chip_init();
  }

  return;
}

// PC7: OVER_TEMP (low-true)
#define OVER_TEMP_PORT              GPIOC
#define OVER_TEMP_PIN               GPIO_PIN_7
#define OVER_TEMP_ASSERTED          GPIO_PIN_RESET
#define OVER_TEMP_DEASSERTED        GPIO_PIN_SET
// PC4: XRP_POWER_GOOD
#define PWRGD_PORT                  GPIOC
#define PWRGD_PIN                   GPIO_PIN_4
#define PWRGD_ASSERTED              GPIO_PIN_SET
#define PWRGD_DEASSERTED            GPIO_PIN_RESET
// PD11: EN_PSU_CH
#define EN_PSU_CH_PORT              GPIOD
#define EN_PSU_CH_PIN               GPIO_PIN_11
#define EN_PSU_CH_ASSERTED          GPIO_PIN_SET
#define EN_PSU_CH_DEASSERTED        GPIO_PIN_RESET
// PC14: PWR_RESET (low-true)
#define PWR_RESET_PORT              GPIOC
#define PWR_RESET_PIN               GPIO_PIN_14
#define PWR_RESET_ASSERTED          GPIO_PIN_RESET
#define PWR_RESET_DEASSERTED        GPIO_PIN_SET

/* Error handling functions: prints errors and logs time and count
 * Only V2 has error handler (todo - implement for Marble Mini)
 */
void marble_error_handler(MarbleErrorCode_t code, uint8_t caller_id) {
    uint8_t idx = (code < ERROR_CODE_COUNT) ? code : ERROR_UNDEFINED;
    uint32_t error_previous_tick = error_last_tick[idx];
    uint32_t tick_milliseconds = marble_get_tick();
    uint64_t total_ms = (uint64_t)tick_overflow_count * (uint64_t)UINT32_MAX + (uint64_t)tick_milliseconds;
    uint32_t total_seconds = total_ms/1000; // overflow after 123 years
    error_counters[idx]++;
    error_last_tick[idx] = total_seconds;
    error_last_caller_id[idx] = caller_id;
    if(!errors_muted) {
      if((error_last_tick[idx] - error_previous_tick > 2) || idx != previous_error){
        printf("\r\033[31m*** MMC ERROR: %s [%d]***\033[0m\r\n", ErrorCodeStrings[idx], caller_id);
        repeating_error = 0xff;
      } else {
        if (idx != repeating_error){
          printf("\r\033[31m*** MMC ERROR: repeating ***\033[0m\r\n> ");
          repeating_error = idx;
        } else {
          if (error_last_tick[idx] - error_previous_tick == 2){ 
            printf(".");
            fflush(stdout);
          }
        }
      }
    }
    previous_error = idx;
}

void marble_check_bringup(void) {
    uint8_t sn[SN_LENGTH];
    int rval = eeprom_read_sn(sn, SN_LENGTH);
    if (rval) {
        printf("Could not find Serial Number\r\n");
        return;
    }
    bool all_zero = true;
    for (size_t i = 0; i < SN_LENGTH; i++) {
        if (sn[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        errors_muted = true;
        printf("Please initialize this board. Error messages are muted.\r\n");
    }
}

void reset_error_repeat(void){
  repeating_error = 0xff;
  previous_error = 0xff;
}

static void print_error_log(void) {
    int any = 0;
    uint32_t tick_milliseconds = marble_get_tick();
    uint64_t total_ms = (uint64_t)tick_overflow_count * (uint64_t)UINT32_MAX + (uint64_t)tick_milliseconds;
    uint32_t total_seconds = total_ms/1000;
    uint32_t seconds_ago = 0;
    for (int i = 0; i < ERROR_CODE_COUNT; i++) {
        if (error_counters[i] > 0) {
            any = 1;
            break;
        }
    }
    if (!any) {
        printf("No errors have occurred since boot\r\n");
        return;
    }
    else {
        printf("Error codes encountered since boot:\r\n");
        for (int i = 0; i < ERROR_CODE_COUNT; i++) {
            if (error_counters[i] > 0) {
                seconds_ago = total_seconds - (unsigned long)error_last_tick[i];
                printf(" - %s: %lu occurrences, last caller [%d], ", ErrorCodeStrings[i], (unsigned long)error_counters[i], error_last_caller_id[i]);
                print_time(seconds_ago);
                printf(" ago\r\n");
            }
        }
    }
    return;
}

/* int board_service(void);
 *  Call in main loop. Handles routines scheduled from interrupts.
 *  Must always return 0 (otherwise execution will terminate).
 */
int board_service(void) {
   // Check state of OVER_TEMP pin
   int gpio = HAL_GPIO_ReadPin(OVER_TEMP_PORT, OVER_TEMP_PIN);
   if ((!_over_temp) && (gpio == OVER_TEMP_ASSERTED)) {
      // Detect asserting edge
      marble_error_handler(ERROR_MARBLE_OVERTEMP, 1);
      _over_temp = 1;
   } else if ((_over_temp) && (gpio == OVER_TEMP_DEASSERTED)) {
      // Detect de-asserting edge
      printf("ALERT: Over-Temperature Condition Cleared\r\n");
      _over_temp = 0;
#ifdef XRP_BYPASS_PWRGD
      _reboot_time = BSP_GET_SYSTICK();
      _do_reboot = 1;
   }
   if (marble_pcb_rev <= Marble_v1_3) {
      if ((_do_reboot) && ((BSP_GET_SYSTICK() - _reboot_time) > XRP_REBOOT_DELAY)) {
         printf("ALERT: Re-initializing after delay.\r\n");
         board_init();
         FPGAWD_SelfReset();
         _pwr_good = 1;
         _do_reboot = 0;
      }
      return 0;
#endif
   }
   if(!_LTM_console_active()) {
      // Check state of PWRGD pin
      gpio = HAL_GPIO_ReadPin(PWRGD_PORT, PWRGD_PIN);
      if (gpio == PWRGD_ASSERTED) {
        if (_pwr_state != PWR_GOOD) {
          if (((++_pwr_state) == PWR_GOOD) && (_pwr_good == 0)) {
            // Detect asserting edge
            printf("ALERT: Power good. Re-initializing.\r\n");
            marble_SLEEP_ms(500); // wait for power to stabilize and peripherals to come up
            _pwr_good = 1;
            board_init();
            FPGAWD_SelfReset();
          } else {
            //printf("PWR STATE CHANGE: _pwr_state = %d;  _pwr_good = %d\r\n", _pwr_state, _pwr_good);
          }
        }
      } else { // gpio == PWRGD_DEASSERTED
        if (_pwr_state != PWR_FAIL) {
          if (((--_pwr_state) == 0) && (_pwr_good > 0)) {
            // Detect de-asserting edge
            marble_error_handler(ERROR_MARBLE_POWERDOWN, 2);
            _pwr_good = 0;
          } else {
            //printf("PWR STATE CHANGE: _pwr_state = %d;  _pwr_good = %d\r\n", _pwr_state, _pwr_good);
          }
        }
      }
   }
#if 0
   // This is unused at the moment.
   if (i2c_pm_alert) {
      printf("TODO - Respond to I2C_PM Alert\r\n");
      i2c_pm_alert = 0;
   }
#endif
   return 0;
}

void marble_print_status(void) {
  printf("_pwr_state = %d\r\n", _pwr_state);
  printf("Board Status:");
  if ((_pwr_good) & (!_over_temp)) {
    printf(" No fault.");
  } else {
    if (_over_temp) printf(" Over-temperature shutdown.");
    if (!_pwr_good) printf(" Lost power (power supply PWRGD deasserted).");
  }
  printf("\r\n");
  print_clock_info();
  print_uptime();
  print_reset_cause();
  print_error_log();
  return;
}

int marble_pwr_good(void) {
  return _pwr_good;
}

Board_Status_t marble_get_status(void) {
  if (_over_temp) return BOARD_STATUS_OVERTEMP;
  if (!_pwr_good) return BOARD_STATUS_POWERDOWN;
  return BOARD_STATUS_GOOD;
}

// Only used in simulation
void cleanup(void) {
   return;
}

/* Initialize UART pins */
void marble_UART_init(void)
{
    printf("+ Init UART...\r\n");
    fflush(stdout);
    /* PA9, PA10 - MMC_CONS_PROG */
    // MX_USART1_UART_Init();
    CONSOLE_USART_Init();
    /* PD5, PD6 - UART4 (Pmod3_7/3_6) */
    // This is disabled to use Pmod3 to drive UI board
    // It only had development use anyhow
    // MX_USART2_UART_Init();
    i2cBusStatus = 0;
}

void pwr_autoboot(void) {
#ifndef NUCLEO
#ifdef XRP_AUTOBOOT
   if (marble_pcb_rev <= Marble_v1_3) {
      printf("XRP_AUTOBOOT\r\n");
      marble_SLEEP_ms(300);
      xrp_boot();
   }
#endif /* XRP_AUTOBOOT */
#endif /* NUCLEO */
   return;
}

/************
* LEDs
************/

#define MAXLEDS 3
#ifdef NUCLEO
#define LED_GPIO  GPIOB
static const uint16_t ledpins[MAXLEDS] = {GPIO_PIN_0, GPIO_PIN_7, GPIO_PIN_14};
#else
static const uint16_t ledpins[MAXLEDS] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2};
#define LED_GPIO  GPIOE
#endif
// NOTE:  ledpins[0] = PE0 = "LD15"
//        ledpins[1] = PE1 = "LD11"
//        ledpins[2] = PE2 = "LD12"

/* Initializes board LED(s) */
static void marble_LED_init(void)
{
   // Handled in MX_GPIO_Init
   return;
}

/* Sets the state of a board LED to on or off */
void marble_LED_set(uint8_t led_num, bool on)
{
  bool state;
#ifdef NUCLEO
  /* On Nucleo, GPIO low is off, high is on */
  state = on;
#else
  /* On Marble, GPIO low is on, high is off */
  state = !on;
#endif
   if (led_num < MAXLEDS) {
      HAL_GPIO_WritePin(LED_GPIO, ledpins[led_num], state);
   }
}

/* Returns the current state of a board LED */
bool marble_LED_get(uint8_t led_num)
{
   bool state = false;

   if (led_num < MAXLEDS) {
      state = HAL_GPIO_ReadPin(LED_GPIO, ledpins[led_num]);
   }

   /* These LEDs are reverse logic. */
   return !state;
}

/* Toggles the current state of a board LED */
void marble_LED_toggle(uint8_t led_num)
{
   if (led_num < MAXLEDS) {
      HAL_GPIO_TogglePin(LED_GPIO, ledpins[led_num]);
   }
}

/* Debug purposes */
void marble_Pmod3_5_write(bool on) {
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return;
}

/************
* FMC & PSU
************/
/* Set FMC power */
void marble_FMC_pwr(bool on)
{
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, on);
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, on);
   return;
}

uint8_t marble_FMC_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_PWR,  HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_10));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_FUSE, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_PWR,  HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_9));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_FUSE, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11));
   return status;
}



static void print_clock_info(void)
{
    uint32_t sysclk_source = __HAL_RCC_GET_SYSCLK_SOURCE();

    // Determine source string
    const char *src_str = "UNKNOWN";
    switch (sysclk_source) {
        case RCC_SYSCLKSOURCE_STATUS_HSI:    src_str = "HSI";  break;
        case RCC_SYSCLKSOURCE_STATUS_HSE:    src_str = "HSE";  break;
        case RCC_SYSCLKSOURCE_STATUS_PLLCLK: src_str = "PLL";  break;
    }

    // Update frequency info
    SystemCoreClockUpdate();

    // Short status print
    printf("Clock: %s @ %lu Hz (PCLK2: %lu Hz)\n",
           src_str,
           HAL_RCC_GetSysClockFreq(),
           HAL_RCC_GetPCLK2Freq());
}


void marble_PSU_pwr(bool on)
{
    printf("+ Init Clocks and power supplies...\r\n        ");
    fflush(stdout);
    marble_SLEEP_ms(10);
    if (on == false) {
        SystemClock_Config_HSI(); // switch to internal clock source, external clock is powered from 3V3!
    }
    // Sch net EN_PSU_CH. Assert when on==true
    HAL_GPIO_WritePin(EN_PSU_CH_PORT, EN_PSU_CH_PIN, on ? EN_PSU_CH_ASSERTED : EN_PSU_CH_DEASSERTED);
    // PSU reset; Power reset pin for LTM4673. Deassert when on==true
    HAL_GPIO_WritePin(PWR_RESET_PORT, PWR_RESET_PIN, on ? PWR_RESET_DEASSERTED : PWR_RESET_ASSERTED);
    if (on) {
        marble_SLEEP_ms(1000); // wait for external oscillator to stabilize
        SystemClock_Config(); // switch to external clock source
    }
    print_clock_info();
    fflush(stdout);
    return;
}

void marble_PSU_reset_write(bool on) {
  // PSU reset; Power reset pin for LTM4673
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, on);
  return;
}

uint8_t marble_PWR_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_PWR_STATUS_PSU_EN, HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11));
   status = WRITE_BIT(status, M_PWR_STATUS_POE,    HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8));
   status = WRITE_BIT(status, M_PWR_STATUS_OTEMP,  HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7));
   return status;
}

void marble_print_GPIO_status(void) {
  // FMC power
  printf("FMC power = ");
  int state = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_9);
  if (state) {
    printf("On\r\n");
  } else {
    printf("Off\r\n");
  }
  state = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11);
  printf("PSU power enable = ");
  if (state) {
    printf("On\r\n");
  } else {
    printf("Off\r\n");
  }
  state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);
  printf("Pmod3_5 = %d", state);
  printf("\r\n");
  if (marble_pcb_rev >= Marble_v1_4) {
    state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_14);
    printf("PSU power reset = ");
    if (state) {
      printf("Asserted\r\n");
    } else {
      printf("Deasserted\r\n");
    }
    state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15);
    printf("PSU power alert = ");
    if (state) {
      // LTM4673 Alert is low-true (open-drain) /Alert
      printf("Deasserted\r\n");
    } else {
      printf("Asserted\r\n");
    }
  }
  return;
}

void marble_list_GPIOs(void) {
  printf("GPIO pins, caps for on, lower case for off\r\n"
         "?) Print state of GPIOs\r\n"
         "a) FMC power\r\n"
         "b) EN_PSU_CH\r\n"
         "c) PB15 J16[4]\r\n");
  if (marble_pcb_rev >= Marble_v1_4) {
    printf("d) PSU reset\r\n"
           "e) PSU alert\r\n");
  }
  return;
}

/************
* Switches and FPGA interrupt
************/

static void marble_SW_init(void)
{
   // SW1 and SW2
   //Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 12, false);
}

bool marble_SW_get(void)
{
   // Pin pulled low on button press
   //if (Chip_GPIO_ReadPortBit(LPC_GPIO, 2, 12) == 0x01) {
   //   return false;
   //}
   return true;
}

bool marble_FPGAint_get(void)
{
   if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3)) {
      return false;
   }
   return true;
}

void disable_fpga(void) {
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, false);
  return;
}

void enable_fpga(void) {
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true);
  return;
}

/************
* GPIO interrupt setup and user-defined handlers
************/
void FPGA_DONE_dummy(void) {}
void (*volatile marble_FPGA_DONE_handler)(void) = FPGA_DONE_dummy;

// Override default (weak) IRQHandler and redirect to HAL shim
void EXTI0_IRQHandler(void)
{
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

// Override default (weak) callback
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
   if (GPIO_Pin == GPIO_PIN_0) { // Handles any interrupt on line 0 (e.g. PA0, PB0, PC0, PD0)
      marble_FPGA_DONE_handler();
   } else if (GPIO_Pin == SMBA_PIN) {
      I2C_PM_smba_handler();
   }
}

// Override default (weak) IRQHandler and redirect to HAL shim
void EXTI15_10_IRQHandler(void) {
   HAL_GPIO_EXTI_IRQHandler(SMBA_PIN);
}

void marble_GPIOint_init(void)
{
  printf("+ Init GPIO Interrupts...\r\n");
  fflush(stdout);
   /*Configure GPIO pin : PD0 - FPGA_DONE rising edge interrupt */
   GPIO_InitTypeDef GPIO_InitStruct = {0};
   GPIO_InitStruct.Pin = GPIO_PIN_0;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Enable interrupt in the NVIC */
   HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
   HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* Register user-defined interrupt handlers */
void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void)) {
   marble_FPGA_DONE_handler = FPGA_DONE_handler;
}

static void I2C_PM_smba_handler(void) {
   i2c_pm_alert = 1;
   return;
}

/*
int marble_I2C_PM_get_alert(void) {
   return i2c_pm_alert;
}

void marble_I2C_PM_clear_alert(void) {
   i2c_pm_alert = 0;
   return;
}
*/

/************
* MGT Multiplexer
************/
static const uint16_t mgtmux_pins[MGT_MAX_PINS] = {GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10};

void marble_MGTMUX_config(uint8_t mgt_msg, uint8_t store, uint8_t print) {
  // Control of FMC power and MGT mux based on mailbox entry MB2_FMC_MGT_CTL
  // Currently addressed as 0x200020 = 2097184 in test_marble_family
  // [1] - FMC_SEL,      [0] - ON/OFF
  // [3] - MGT_MUX1_SEL, [2] - ON/OFF
  // [5] - MGT_MUX2_SEL, [4] - ON/OFF
  // [7] - MGT_MUX3_SEL, [6] - ON/OFF
  if ((mgt_msg & 0xaa) == 0) {
    // Nothing to do; early exit
    return;
  }
  if (print) {
    printf("Setting ");
  }
  if (mgt_msg & 0x2) {
    if (print) {
      printf("FMC_Pwr=");
      if (mgt_msg & 0x1) {
        printf("ON ");
      } else {
        printf("OFF ");
      }
    }
    marble_FMC_pwr(mgt_msg & 1);
  }
  uint8_t v;
  if (mgt_msg & 0x8) {
    v = ((mgt_msg & 0x4) >> 2);
    if (print) {
      printf("MUX1=%d ", v);
    }
    HAL_GPIO_WritePin(GPIOE, mgtmux_pins[0], v);
    //marble_MGTMUX_set(1, v);
  }
  if (mgt_msg & 0x20) {
    v = ((mgt_msg & 0x10) >> 4);
    if (print) {
      printf("MUX2=%d ", v);
    }
    //marble_MGTMUX_set(2, v);
    HAL_GPIO_WritePin(GPIOE, mgtmux_pins[1], v);
  }
  if (mgt_msg & 0x80) {
    v = ((mgt_msg & 0x40) >> 6);
    if (print) {
      printf("MUX3=%d ", v);
    }
    //marble_MGTMUX_set(3, v & 1);
    HAL_GPIO_WritePin(GPIOE, mgtmux_pins[2], v);
  }
  // Store to EEPROM
  if (store) {
    marble_MGTMUX_store();
  }
  if (print) {
    printf("\r\n");
  }
  return;
}

void marble_MGTMUX_set(uint8_t mgt_num, bool on) {
  mgt_num -= 1;
  if (mgt_num < MGT_MAX_PINS) {
     HAL_GPIO_WritePin(GPIOE, mgtmux_pins[mgt_num], on);
  }
  // Store to EEPROM
  marble_MGTMUX_store();
  return;
}

uint8_t marble_MGTMUX_status(void) {
   uint8_t mgt_cfg = 0;
   for (unsigned i=0; i < MGT_MAX_PINS; i++) {
      mgt_cfg |= HAL_GPIO_ReadPin(GPIOE, mgtmux_pins[i])<<i;
   }
   return mgt_cfg;
}

void marble_MGTMUX_set_all(uint8_t mgt_cfg) {
  // Set the state of all MGT_MUX pins simultaneously.
  // This function uses the same bit format as marble_MGTMUX_status()
  // I.e. marble_MGTMUX_set_all(marble_MGTMUX_status()) is no-op.

  // Note! Non-portable function depends on MGT_MUX pins being consecutive in the GPIO.ODR register
  // mgt_cfg bit  Signal
  // -------------------
  // Bit0         MUX1
  // Bit1         MUX2
  // Bit2         MUX3
  uint32_t odr = GPIOE->ODR;
  uint32_t mask = (uint32_t)(7 << 8);
  GPIOE->ODR = (odr & ~mask) | ((mgt_cfg << 8) & mask);
  // NOTE! Does not store to EEPROM because this is used exclusively to apply stored value at startup
  return;
}

// Store to non-volatile memory
static int marble_MGTMUX_store(void) {
  uint8_t mgt_cfg = marble_MGTMUX_status();
  int rval = eeprom_store_mgt_mux((const uint8_t *)&mgt_cfg, 1);
  if (rval) {
    printf("Failed to store MGTMUX to EEPROM. Error = %d\r\n", rval);
  }
  return rval;
}

/************
* I2C
************/
#define SPEED_100KHZ 100000
#define I2C_TIMEOUT_MS 10

/* Non-destructive I2C probe function based on empty data command, i.e. S+[A,RW]+P */
int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr) {
   int rc = HAL_I2C_IsDeviceReady(I2C_bus, addr, 2, 2);
   i2cBusStatus |= rc;
   return rc;
}

static void marble_I2C_error_handler(I2C_BUS I2C_bus, int rc) {
    bool isFPGA = (I2C_bus == I2C_FPGA);
    bool isPM   = (I2C_bus == I2C_PM);
    //i2c_error_counter++;
    // Handle function return codes first
    switch (rc) {
        case HAL_TIMEOUT:{
            marble_error_handler(isFPGA ? ERROR_I2C_FPGA_TIMEOUT : 
                        isPM   ? ERROR_I2C_PM_TIMEOUT   : ERROR_UNDEFINED, 3);
            break;
          }
        case HAL_BUSY:{
            marble_error_handler(isFPGA ? ERROR_I2C_FPGA_BUSY : 
                        isPM   ? ERROR_I2C_PM_BUSY   : ERROR_UNDEFINED, 4);
            break;
          }
        case HAL_ERROR: {
            // If we reach here, rc == HAL_ERROR, so check HAL error flags
            uint32_t halErr = I2C_bus->ErrorCode;

            // Force STOP condition on HAL error
            I2C_bus->Instance->CR1 |= I2C_CR1_STOP;

            // Map each HAL error bit to our MarbleErrorCode_t and log it
            if (halErr & HAL_I2C_ERROR_NONE)
                marble_error_handler(isFPGA ? ERROR_I2C_FPGA_NONE : ERROR_I2C_PM_NONE, 5);
            if (halErr & HAL_I2C_ERROR_BERR)
                marble_error_handler(isFPGA ? ERROR_I2C_FPGA_BERR : ERROR_I2C_PM_BERR, 6);
            if (halErr & HAL_I2C_ERROR_ARLO)
                marble_error_handler(isFPGA ? ERROR_I2C_FPGA_ARLO : ERROR_I2C_PM_ARLO, 7);
            if (halErr & HAL_I2C_ERROR_AF)
                marble_error_handler(isFPGA ? ERROR_I2C_FPGA_AF : ERROR_I2C_PM_AF, 8);
            if (halErr & HAL_I2C_ERROR_OVR)
                marble_error_handler(isFPGA ? ERROR_I2C_FPGA_OVR : ERROR_I2C_PM_OVR, 9);
            if (halErr & HAL_I2C_ERROR_DMA)
                marble_error_handler(isFPGA ? ERROR_I2C_FPGA_DMA : ERROR_I2C_PM_DMA, 10);
            if (halErr & HAL_I2C_ERROR_TIMEOUT)
                marble_error_handler(isFPGA ? ERROR_I2C_FPGA_TIMEOUT : ERROR_I2C_PM_TIMEOUT, 11);
            break;
          }
        default:
            i2c_error_counter = 0;
            break;
      }
}

static int marble_I2C_bus_prepare(I2C_BUS I2C_bus) {
  // first make sure that the bus is available
  int i=0;
  while(((HAL_I2C_GetState(I2C_bus) != HAL_I2C_STATE_READY)||(__HAL_I2C_GET_FLAG(I2C_bus, I2C_FLAG_BUSY))) && (i<50)) {
    marble_SLEEP_us(10); //sleep one i2c clock cycle at 100kHz
    i++;
    //printf("Warning: I2C hardware busy (flag 0x%08X)\r\n", (__HAL_I2C_GET_FLAG(I2C_bus, I2C_FLAG_BUSY)));
  }
  if((i >= 50) || i2c_error_counter >=5) { //after 0.5ms, timeout
    bool isFPGA = (I2C_bus == I2C_FPGA);
    i2c_error_counter++;
    marble_error_handler(isFPGA ? ERROR_I2C_FPGA_HW_BUSY : ERROR_I2C_PM_HW_BUSY, 12);
    printf("Error: Timeout - I2C hardware busy (flag 0x%08X), I2C bus state %d\r\n", (__HAL_I2C_GET_FLAG(I2C_bus, I2C_FLAG_BUSY)), HAL_I2C_GetState(I2C_bus));
    if (i2c_error_counter >= 5) {
      marble_error_handler(isFPGA ? ERROR_I2C_FPGA_LOCKUP : ERROR_I2C_PM_LOCKUP, 13);
      printf("%s appears to be stuck. Attempting re-init...\r\n",
        I2C_bus->Instance == I2C1 ? "I2C1" :
        I2C_bus->Instance == I2C2 ? "I2C2" :
        I2C_bus->Instance == I2C3 ? "I2C3" : "Unknown I2C Bus");
      MX_I2C_BusReInit(I2C_bus);
      i2c_error_counter = 0;
    }
    return 1; // might want a different return value
  }
  return 0;
}

/* Generic I2C send function with selectable I2C bus and 8-bit I2C addresses (R/W bit = 0) */
/* 1-byte register addresses */
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size) {
  // first make sure that the bus is available
  if(marble_I2C_bus_prepare(I2C_bus) != 0) {
    return 1; // bus not available
  }
  // I2C action and error handling
  int rc = HAL_I2C_Master_Transmit(I2C_bus, (uint16_t)addr, data, size, I2C_TIMEOUT_MS);
  marble_I2C_error_handler(I2C_bus, rc);
  i2cBusStatus |= rc;
  if (rc == HAL_OK) {
    // rnw=0, cmd=-1
    i2c_hook(I2C_bus, addr, 0, -1, data, size);
  }
  else{
    printd("rc = %d\r\n", rc);
    printd("marble_I2C_send(addr 0x%2x, data 0x%2x, size %d)\r\n", addr, (unsigned int)*data, size);
  }
  return rc;
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, const uint8_t *data, int size) {
  // first make sure that the bus is available
  if(marble_I2C_bus_prepare(I2C_bus) != 0) {
    return 1; // bus not available
  }
  // I2C action and error handling
  int rc = HAL_I2C_Mem_Write(I2C_bus, (uint16_t)addr, cmd, 1, (uint8_t *)data, size, I2C_TIMEOUT_MS);
  marble_I2C_error_handler(I2C_bus, rc);
  if (rc == HAL_OK) {
    // rnw=0, cmd=cmd
    i2c_hook(I2C_bus, addr, 0, cmd, data, size);
  }
  else{
    printd("rc = %d\r\n", rc);
    printd("HAL_I2C_Mem_Write(addr 0x%2x, data 0x%2x, size %d)\r\n", addr, (unsigned int)*data, size);
  }
  i2cBusStatus |= rc;
  return rc;
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
  // first make sure that the bus is available
  if(marble_I2C_bus_prepare(I2C_bus) != 0) {
    return 1; // bus not available
  }
  // I2C action and error handling
  int rc = HAL_I2C_Master_Receive(I2C_bus, (uint16_t)addr, data, size, I2C_TIMEOUT_MS);
  marble_I2C_error_handler(I2C_bus, rc);
  i2cBusStatus |= rc;
  if (rc == HAL_OK) {
    // rnw=1, cmd=-1
    i2c_hook(I2C_bus, addr, 1, -1, data, size);
  }
  else{
    printd("rc = %d\r\n", rc);
    printd("marble_I2C_recv(addr 0x%2x, data 0x%2x, size %d)\r\n", addr, (unsigned int)*data, size);
  }
  return rc;
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
  // first make sure that the bus is available
  if(marble_I2C_bus_prepare(I2C_bus) != 0) {
    return 1; // bus not available
  }
  // I2C action and error handling
  int rc = HAL_I2C_Mem_Read(I2C_bus, (uint16_t)addr, cmd, 1, data, size, I2C_TIMEOUT_MS);
  marble_I2C_error_handler(I2C_bus, rc);
  i2cBusStatus |= rc;
  if (rc == HAL_OK) {
    // rnw=1, cmd=cmd
    i2c_hook(I2C_bus, addr, 1, cmd, data, size);
  }
  else{
    printd("rc = %d\r\n", rc);
    printd("marble_I2C_cmdrecv(addr 0x%2x, data 0x%2x, size %d)\r\n", addr, (unsigned int)*data, size);
  }
  return rc;
}

/* Same but 2-byte register addresses */
int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, const uint8_t *data, int size) {
  // first make sure that the bus is available
  if(marble_I2C_bus_prepare(I2C_bus) != 0) {
    return 1; // bus not available
  }
  // I2C action and error handling
  int rc = HAL_I2C_Mem_Write(I2C_bus, (uint16_t)addr, cmd, 2, (uint8_t *)data, size, I2C_TIMEOUT_MS);
  marble_I2C_error_handler(I2C_bus, rc);
  i2cBusStatus |= rc;
  if (rc == HAL_OK) {
    // rnw=0, cmd=cmd
    i2c_hook(I2C_bus, addr, 0, cmd, data, size);
  }
  else{
    printd("rc = %d\r\n", rc);
    printd("marble_I2C_cmdsend_a2(addr 0x%2x, data 0x%2x, size %d)\r\n", addr, (unsigned int)*data, size);
  }
  return rc;
}
int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
  // first make sure that the bus is available
  if(marble_I2C_bus_prepare(I2C_bus) != 0) {
    return 1; // bus not available
  }
  // I2C action and error handling
   int rc = HAL_I2C_Mem_Read(I2C_bus, (uint16_t)addr, cmd, 2, data, size, I2C_TIMEOUT_MS);
   marble_I2C_error_handler(I2C_bus, rc);
   i2cBusStatus |= rc;
   if (rc == HAL_OK) {
      // rnw=1, cmd=cmd
      i2c_hook(I2C_bus, addr, 1, cmd, data, size);
   }
   else{
    printd("rc = %d\r\n", rc);
    printd("marble_I2C_cmdrecv_a2(addr 0x%2x, data 0x%2x, size %d)\r\n", addr, (unsigned int)*data, size);
  }
   return rc;
}

/* static int i2c_hook(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw,
                       int cmd, const uint8_t *data, int len);
 *  Callback (hook) function for side-effects of I2C transactions.
 *  In blocking mode, this function is called AFTER a successful return of the I2C_write
 *  or I2C_read functions.  In non-blocking mode, this callback should be scheduled to
 *  run in the I2C_transaction_complete interrupt handler and should only be called
 *  in thread mode (not called from the ISR).
 *  @params:
 *    I2C_BUS I2C_bus: The bus on which the transaction occurred. One of I2C_PM,
 *                  or I2C_FPGA.
 *    uint8_t addr: 8-bit (not 7-bit) I2C device address of the transaction
 *    uint8_t rnw:  Transaction direction. 0=Write (central to periph), 1=Read
 *    int cmd:      For API with explicit command/reg bytes, 0 <= cmd <= 0xff.
 *                  For API with 2-byte commands, 0 <= cmd <= 0xffff.
 *                  For API without command/reg bytes, cmd = -1. If a cmd byte
 *                  is mandatory for a given device, the associated hook should
 *                  expect data[0] = cmd_byte (or data[0:1] = cmd_halfword) with
 *                  the transaction data continuing immediately afterward.
 *    uint8_t *data: For Read, the data returned by peripheral. For Write, the
 *                  data sent to peripheral.
 *    int len:      The length of valid data in 'data' pointer.
 */
static int i2c_hook(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw,
                    int cmd, const uint8_t *data, int len)
{
   if (I2C_bus == I2C_PM) {
      i2c_pm_hook(addr, rnw, cmd, data, len);
   }
   // Bus-specific I2C hooks here
   return 0;
}

/************
* SSP/SPI
************/
static void SPI_CSB_SET(SSP_PORT ssp, bool set);

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
  SPI_CSB_SET(ssp, false);
  int rc = HAL_SPI_Transmit(ssp, (uint8_t*) buffer, size, HAL_MAX_DELAY);
  SPI_CSB_SET(ssp, true);
  return rc;
}

int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
  SPI_CSB_SET(ssp, false);
  int rc = HAL_SPI_Receive(ssp, (uint8_t*) buffer, size, HAL_MAX_DELAY);
  SPI_CSB_SET(ssp, true);
  return rc;
}

int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size)
{
  SPI_CSB_SET(ssp, false);
  int rc = HAL_SPI_TransmitReceive(ssp, (uint8_t*) tx_buf, (uint8_t*) rx_buf,size, HAL_MAX_DELAY);
  SPI_CSB_SET(ssp, true);
  return rc;
}

/************
* MDIO to PHY
************/
void marble_MDIO_init(void)
{
  //Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_ENET);

  /* Setup MII clock rate and PHY address */
  //Chip_ENET_SetupMII(LPC_ETHERNET, Chip_ENET_FindMIIDiv(LPC_ETHERNET, 2500000), 0);
}

void marble_MDIO_write(uint16_t reg, uint32_t data)
{
  //Chip_ENET_StartMIIWrite(LPC_ETHERNET, reg, data);
  HAL_ETH_WritePHYRegister(&heth, reg, data);
  //while (Chip_ENET_IsMIIBusy(LPC_ETHERNET));
}

uint32_t marble_MDIO_read(uint16_t reg)
{
  uint32_t value;
  HAL_ETH_ReadPHYRegister(&heth, reg, &value);
  return value;
}

/************
* System Timer and Stopwatch
************/
void SysTick_Handler_dummy(void) {}
void (*volatile marble_SysTick_Handler)(void) = SysTick_Handler_dummy;

// Override default (weak) SysTick_Handler
void SysTick_Handler(void)
{
  HAL_IncTick(); // Advances HAL timebase used in HAL_Delay
  if (marble_SysTick_Handler)
    marble_SysTick_Handler();
}

uint32_t marble_get_tick(void) {
  uint32_t previousTick = 0;
  uint32_t newTick = (uint32_t)HAL_GetTick();
  if (previousTick > newTick)
    tick_overflow_count++;
  return newTick;
}

/* Register user-defined interrupt handlers */
void marble_SYSTIMER_handler(void (*handler)(void)) {
   marble_SysTick_Handler = handler;
}

/* Configures 24-bit count-down timer and enables systimer interrupt */
uint32_t marble_SYSTIMER_ms(uint32_t delay)
{
  // WARNING: Hardcoded to 1 ms since this is what increments HAL_IncTick() and
  // enables HAL_Delay
  delay = 1; // TODO: Consider decoupling stopwatch from system timer

  const uint32_t MAX_TICKS = (1<<24)-1;
  const uint32_t MAX_DELAY_MS = (SystemCoreClock * 1000U) / MAX_TICKS;
  uint32_t ticks = (SystemCoreClock / 1000U) * delay;
  if (delay > MAX_DELAY_MS) {
    ticks = MAX_TICKS;
    delay = MAX_DELAY_MS;
  }
  SysTick_Config(ticks);
  return delay;
}

void marble_SLEEP_ms(uint32_t delay)
{
  HAL_Delay(delay); // TODO: Consider replacing with stopwatch built from timer peripheral
}

void marble_SLEEP_us(uint32_t delay)
{
  (void) delay;
  return; // XXX Not available unless HAL weak definitions are overridden
  // Good thing nobody depends on this (yet)
}

/************
* Board Init
************/

uint32_t marble_init(void)
{
  HAL_Init();
  SystemClock_Config_HSI();
  marble_UART_init();
  MX_GPIO_Init();
  marble_GPIOint_init();
  marble_read_pcb_rev();
  get_MMC_SN();
  get_Marble_SN();
  marble_PSU_pwr(true);
  printf("        PSU and clocks initialized (%ld)\r\n", marble_get_tick());
  MX_ETH_MDIO_Init();

  printf("+ Init I2C FPGA interface...\r\n");
  MX_I2C_BusInit(&hi2c1, &I2C_FPGA, I2C1);
  printf("+ Init I2C PM interface...\r\n");
  MX_I2C_BusInit(&hi2c3, &I2C_PM, I2C3);
  fflush(stdout);

  MX_SPI1_Init();
  RNG_Init();
  reset_cause = reset_cause_get();

  marble_LED_init(); // empty function
  marble_SW_init();// empty function

  // Init SSP busses
  //marble_SSP_init(LPC_SSP0);
  //marble_SSP_init(LPC_SSP1);
  return 0;
}

void marble_print_ID_status(int len) {
  if(len ==2){
  #ifdef NUCLEO
    printf("PCB Rev: Nucleo\r\n");
  #else
    switch (marble_pcb_rev) {
      case Marble_v1_3:
        printf("PCB Rev: Marble v1.3\r\n");
        break;
      case Marble_v1_4:
        printf("PCB Rev: Marble v1.4\r\n");
        //printf("PCB Rev: Marble v1.4\r\n");
        break;
      case Marble_v1_5:
        printf("PCB Rev: Marble v1.5\r\n");
        break;
      case Marble_v1_6:
        printf("PCB Rev: Marble v1.6\r\n");
        break;
      case Marble_v1_7:
        printf("PCB Rev: Marble v1.7\r\n");
        break;
      case Marble_v1_8:
        printf("PCB Rev: Marble v1.8\r\n");
        break;
      case Marble_v1_9:
        printf("PCB Rev: Marble v1.9\r\n");
        break;
      default:
        printf("PCB Rev: Marble v1.2\r\n");
        break;
    }
  #endif
    console_print_SN();
    show_MMC_SN();
    show_mmc_ID();
    show_PHY_ID();
    printf("Firmware revision: " GIT_REV " [Git]\r\n");// placeholder for GIT_REV
    printf("MMC Boot ID: 0x%08lX\n", boot_id);
    console_print_mac_ip();
    // print_clock_info();
    // print_uptime();
    // print_reset_cause();
    // print_error_log();
    return;
  } else {
    printf("%s",unk_str);
  }
}

Marble_PCB_Rev_t marble_get_pcb_rev(void) {
  return marble_pcb_rev;
}

uint8_t marble_get_board_id(void) {
  return ((uint8_t)(marble_pcb_rev & 0x0F) | BOARD_TYPE_MARBLE);
}

// This macro yields a number in the range 0-15 inclusive where the original
// bits 12-15 end up reversed and shifted, as in: (MSB->LSB) |b12|b13|b14|b15|
#define MARBLE_PCB_REV_XFORM(gpio_idr)    ((__RBIT(gpio_idr) >> 16) & 0xF)
static void marble_read_pcb_rev(void) {
  printf("+ Read Marble PCB Rev...\r\n");
  fflush(stdout);
  uint32_t pcbid = MARBLE_PCB_REV_XFORM(GPIOD->IDR);
  // Explicit case check rather than simple cast to catch unenumerated values
  // in 'default'
  switch ((Marble_PCB_Rev_t)pcbid) {
    case Marble_v1_3:
      marble_pcb_rev = Marble_v1_3;
      break;
    case Marble_v1_4:
      marble_pcb_rev = Marble_v1_4;
      break;
    case Marble_v1_5: // Future support
      marble_pcb_rev = Marble_v1_5;
      break;
    case Marble_v1_6: // Future support
      marble_pcb_rev = Marble_v1_6;
      break;
    case Marble_v1_7: // Future support
      marble_pcb_rev = Marble_v1_7;
      break;
    case Marble_v1_8: // Future support
      marble_pcb_rev = Marble_v1_8;
      break;
    case Marble_v1_9: // Future support
      marble_pcb_rev = Marble_v1_9;
      break;
    default:
      marble_pcb_rev = Marble_v1_2;
      break;
  }
  return;
}

// Read unique 32-bit ID (from 96-bit identifier)
static void get_MMC_SN(void) {
  HAL_GetUID(MMC_SN);
}

static void show_MMC_SN(void) {
   printf("MMC Serial Number: 0x%08lX%08lX%08lX\r\n", MMC_SN[2], MMC_SN[1], MMC_SN[0]);
}

static void get_Marble_SN(void) {
  Marble_SN = 0;
}

static void SystemClock_Config(void)
{
  // HSE_STARTUP_TIMEOUT defined in stm32f2xx_hal_conf.h defines timeout for HSE start-up
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  uint32_t FLatency;

  /* Ensure SYSCLK is not currently PLL before touching PLL config */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct, &FLatency);

  /* Switch SYSCLK to HSI temporarily */
  RCC_ClkInitStruct.ClockType    = RCC_CLOCKTYPE_SYSCLK;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLatency) != HAL_OK)
  {
    marble_error_handler(ERROR_RCC_CLOCK_CONFIG, 14);
  }

/* Configure and enable external oscillator (HSE) and PLL */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = CONFIG_CLK_PLLM;
  RCC_OscInitStruct.PLL.PLLN = CONFIG_CLK_PLLN;
  RCC_OscInitStruct.PLL.PLLP = CONFIG_CLK_PLLP;
  RCC_OscInitStruct.PLL.PLLQ = CONFIG_CLK_PLLQ;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
      uint32_t cr  = RCC->CR;
      printf("RCC->CR = 0x%08lX\r\n", cr);
      if (!(cr & RCC_CR_HSERDY))
          printf("HSE failed to start or stabilize\r\n");

      if ((cr & RCC_CR_HSERDY) && !(cr & RCC_CR_PLLRDY))
          printf("PLL failed to lock\r\n");
      marble_error_handler(ERROR_RCC_OSC_CONFIG, 15);
   }
  /* Initialize the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = AHBCLK_DIV;
  RCC_ClkInitStruct.APB1CLKDivider = APB1CLK_DIV;
  RCC_ClkInitStruct.APB2CLKDivider = APB2CLK_DIV;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    marble_error_handler(ERROR_RCC_CLOCK_CONFIG, 16);
  }
}

void SystemClock_Config_HSI(void)  // switch to internal clock source, external clock is powered from 3V3!
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  uint32_t FLatency;

  /* Ensure SYSCLK is not currently PLL before touching PLL config */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct, &FLatency);

  /* Switch SYSCLK to HSI temporarily */
  RCC_ClkInitStruct.ClockType    = RCC_CLOCKTYPE_SYSCLK;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLatency) != HAL_OK)
  {
      marble_error_handler(ERROR_RCC_CLOCK_CONFIG, 17);
  }

  /* Configure and enable external oscillator (HSE) and PLL */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 13;
  RCC_OscInitStruct.PLL.PLLN = 195;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    uint32_t cr  = RCC->CR;
    printf("RCC->CR = 0x%08lX\r\n", cr);
    if (!(cr & RCC_CR_HSERDY))
        printf("HSE failed to start or stabilize\r\n");

    if ((cr & RCC_CR_HSERDY) && !(cr & RCC_CR_PLLRDY))
        printf("PLL failed to lock\r\n");
    marble_error_handler(ERROR_RCC_OSC_CONFIG, 18);
  }
  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    marble_error_handler(ERROR_RCC_CLOCK_CONFIG, 19);
  }
}

static void MX_ETH_MDIO_Init(void)
{
    printf("+ Init Ethernet MDIO interface...\r\n");
    fflush(stdout);
    // Enable ETH clock (needed for MDIO hardware)
    __HAL_RCC_ETH_CLK_ENABLE();

    // ETH handle just needs the Instance and PhyAddress for MDIO ops
    heth.Instance = ETH;
    heth.Init.PhyAddress = PHY_USER_NAME_PHY_ADDRESS;

    __HAL_ETH_RESET_HANDLE_STATE(&heth);
    HAL_ETH_MspInit(&heth);
    marble_SLEEP_ms(100);
    uint32_t id1, id2;
    if (HAL_ETH_ReadPHYRegister(&heth, MDIO_PHY_REG_PHY_ID_1, &id1) == HAL_OK &&
        HAL_ETH_ReadPHYRegister(&heth, MDIO_PHY_REG_PHY_ID_2, &id2) == HAL_OK)
    {
        if(id1 != 0x0141){
          printf("PHY ID: 0x%04lX 0x%04lX\n", id1, id2);
          marble_error_handler(ERROR_ETH_MDIO_ID, 20);
        }
    }
    else
    {
        marble_error_handler(ERROR_ETH_MDIO_INIT, 21);
    }
}

static void MX_I2C_BusInit(I2C_HandleTypeDef *hi2c, I2C_BUS *bus, I2C_TypeDef *instance)
{
    hi2c->Instance = instance;
    hi2c->Init.ClockSpeed = SPEED_100KHZ;
    hi2c->Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c->Init.OwnAddress1 = 0;
    hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c->Init.OwnAddress2 = 0;
    hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(hi2c) != HAL_OK)
    {
        if (hi2c->Instance == I2C1) {
            marble_error_handler(ERROR_I2C1_INIT, 22);
        } else if (hi2c->Instance == I2C3) {
            marble_error_handler(ERROR_I2C3_INIT, 23);
        } else {
            marble_error_handler(ERROR_UNDEFINED, 24);
        }
    }
    *bus = hi2c;
    marble_SLEEP_ms(1); // settle and print
}

static void MX_I2C_BusReInit(I2C_BUS I2C_bus)
{
    if (HAL_I2C_DeInit(I2C_bus) != HAL_OK)
    {
        if (I2C_bus->Instance == I2C1) {
            marble_error_handler(ERROR_I2C1_DEINIT, 25);
        } else if (I2C_bus->Instance == I2C3) {
            marble_error_handler(ERROR_I2C3_DEINIT, 26);
        } else {
            marble_error_handler(ERROR_UNDEFINED, 27);
        }
    }
    printf("Flushing the bus...\r\n");
    I2C_ClearBus(I2C_bus);
    if (HAL_I2C_Init(I2C_bus) != HAL_OK)
    {
        if (I2C_bus->Instance == I2C1) {
            marble_error_handler(ERROR_I2C1_INIT, 28);
        } else if (I2C_bus->Instance == I2C3) {
            marble_error_handler(ERROR_I2C3_INIT, 29);
        } else {
            marble_error_handler(ERROR_UNDEFINED, 30);
        }
    }
    else {
        printf("Bus re-init successful.\r\n");
    }
}

static void I2C_ClearBus(I2C_BUS I2C_bus)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_TypeDef *sclPort, *sdaPort;
    uint16_t sclPin, sdaPin;

    // Select SCL/SDA pins for the I2C instance
    if (I2C_bus->Instance == I2C1) { // see HAL_I2C_MspInit for pin mapping
        __HAL_RCC_GPIOB_CLK_ENABLE();
        sclPort = GPIOB; sclPin = GPIO_PIN_6;
        sdaPort = GPIOB; sdaPin = GPIO_PIN_7;
    } else if (I2C_bus->Instance == I2C3) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        sclPort = GPIOA; sclPin = GPIO_PIN_8;
        sdaPort = GPIOC; sdaPin = GPIO_PIN_9;
    } else {
        return;
    }

    // Configure SCL and SDA as GPIO open-drain outputs
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = sclPin;
    HAL_GPIO_Init(sclPort, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = sdaPin;
    HAL_GPIO_Init(sdaPort, &GPIO_InitStruct);

    // Clock SCL up to 9 pulses if SDA is held low
    for (int i = 0; i < 9 && HAL_GPIO_ReadPin(sdaPort, sdaPin) == GPIO_PIN_RESET; i++) {
        HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_SET);
        marble_SLEEP_us(5);
        HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_RESET);
        marble_SLEEP_us(5);
    }

    // Generate STOP condition: SCL high then SDA high
    HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_SET);
    marble_SLEEP_us(5);
    HAL_GPIO_WritePin(sdaPort, sdaPin, GPIO_PIN_SET);
    marble_SLEEP_us(5);

    // Restore pins to default (de-init GPIO so I2C AF can reconfigure)
    HAL_GPIO_DeInit(sclPort, sclPin);
    HAL_GPIO_DeInit(sdaPort, sdaPin);
}

static void MX_SPI1_Init(void)
{
    printf("+ Init SPI interface...\r\n");
    fflush(stdout);
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        marble_error_handler(ERROR_SPI1_INIT, 31);
    }
    SSP_FPGA = &hspi1;
}

/*
static void MX_SPI2_Init(void)
{
   hspi2.Instance = SPI2;
   hspi2.Init.Mode = SPI_MODE_MASTER;
   hspi2.Init.Direction = SPI_DIRECTION_2LINES;
   hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
   hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
   hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
   hspi2.Init.NSS = SPI_NSS_SOFT;
   hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
   hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
   hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
   hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
   hspi2.Init.CRCPolynomial = 10;
   if (HAL_SPI_Init(&hspi2) != HAL_OK)
   {
      marble_error_handler();
   }
   SSP_PMOD = &hspi2;
}
*/

static void SPI_CSB_SET(SSP_PORT ssp, bool set)
{
    if (ssp == SSP_FPGA) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

/*
static void MX_USART1_UART_Init(void)
{
   huart_console.Instance = USART1;
   huart_console.Init.BaudRate = 115200;
   huart_console.Init.WordLength = UART_WORDLENGTH_8B;
   huart_console.Init.StopBits = UART_STOPBITS_1;
   huart_console.Init.Parity = UART_PARITY_NONE;
   huart_console.Init.Mode = UART_MODE_TX_RX;
   huart_console.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart_console.Init.OverSampling = UART_OVERSAMPLING_16;
   if (HAL_UART_Init(&huart_console) != HAL_OK)
   {
      marble_error_handler();
   }
   // Enable RXNE, TXE interrupts
   SET_BIT(huart_console.Instance->CR1, USART_CR1_RXNEIE);
}
*/

static void RNG_Init(void){
  printf("+ Init random number generator...\r\n");
  fflush(stdout);
  hrng.Instance = RNG;
  __HAL_RCC_RNG_CLK_ENABLE();
  rng_init_status = HAL_RNG_Init(&hrng);
  get_hw_rnd(&boot_id);
}

static void CONSOLE_USART_Init(void) {
#ifdef NUCLEO
  huart_console.Instance = USART3;
#else
  huart_console.Instance = USART1;
#endif
  huart_console.Init.BaudRate = 115200;
  huart_console.Init.WordLength = UART_WORDLENGTH_8B;
  huart_console.Init.StopBits = UART_STOPBITS_1;
  huart_console.Init.Parity = UART_PARITY_NONE;
  huart_console.Init.Mode = UART_MODE_TX_RX;
  huart_console.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart_console.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart_console) != HAL_OK)
  {
     marble_error_handler(ERROR_UART_CONSOLE_INIT, 32);
  }
  // Enable RXNE, TXE interrupts
  SET_BIT(CONSOLE_USART->CR1, USART_CR1_RXNEIE);
  return;
}

/*
static void MX_USART2_UART_Init(void)
{
   huart2.Instance = USART2;
   huart2.Init.BaudRate = 115200;
   huart2.Init.WordLength = UART_WORDLENGTH_8B;
   huart2.Init.StopBits = UART_STOPBITS_1;
   huart2.Init.Parity = UART_PARITY_NONE;
   huart2.Init.Mode = UART_MODE_TX_RX;
   huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart2.Init.OverSampling = UART_OVERSAMPLING_16;
   if (HAL_UART_Init(&huart2) != HAL_OK)
   {
      marble_error_handler();
   }
}
*/

static void MX_GPIO_Init(void)
{
   printf("+ Init GPIO...\r\n");
   fflush(stdout);
   GPIO_InitTypeDef GPIO_InitStruct = {0};

   /* GPIO Ports Clock Enable */
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOH_CLK_ENABLE();
   __HAL_RCC_GPIOC_CLK_ENABLE();
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOE,GPIO_PIN_0|GPIO_PIN_1| GPIO_PIN_2|GPIO_PIN_5|GPIO_PIN_8|GPIO_PIN_9
                           |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                           |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
                           |GPIO_PIN_2, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);

   /*Configure GPIO pins : PE2 PE8 PE9
                            PE10 PE11 PE12 PE13
                            PE14 PE15 PE0 PE1 */
   GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_8|GPIO_PIN_9
                           |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                           |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

   /*Configure GPIO pins : PE5 (FPGA_RESETn) */
   GPIO_InitStruct.Pin = GPIO_PIN_5;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, true);
   HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

   /*Configure GPIO pins : PC7 PC8 */
   GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // GPIO_MODE_IT_FALLING
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   /* Configure GPIO pin PWRGD */
   GPIO_InitStruct.Pin = PWRGD_PIN; // GPIO_PIN_4
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_PULLDOWN;
   GPIO_InitStruct.Pull = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(PWRGD_PORT, &GPIO_InitStruct);

   /*Configure GPIO pins : PC0 - PROG_B */
   GPIO_InitStruct.Pin = GPIO_PIN_0;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

   // N.B.: Order matters here; GPIO state must be set before it's init'd
   // so that there's no temporary glitch that pulls low PROG_B and thus
   // resets the FPGA
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true);
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   /*Configure GPIO pin : PA0, PA1, PA7 */
   GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   /*Configure GPIO pin : PA3 - FPGA_INT (just an input for now; not an interrupt) */
   GPIO_InitStruct.Pin = GPIO_PIN_3;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   /*Configure GPIO pins : PB0 PB1 PB2 */
   GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /*Configure GPIO pin : PB14 */
   GPIO_InitStruct.Pin = GPIO_PIN_14;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /*Configure GPIO pin : PB15 */
   GPIO_InitStruct.Pin = GPIO_PIN_15;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // TEST
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /* Configure GPIO pins : PD9 PD10 PD11 PD1 PD2 */
   // GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1|GPIO_PIN_2;
   GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
                           |GPIO_PIN_2;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pins : Marble PCB Rev ID (PD12 PD13 PD14 PD15) */
   GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pins : PD3 PD4 */
   GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pin : PC12 PC14 */
   GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_14;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   /* Configure GPIO pin : PC15 */
   GPIO_InitStruct.Pin = SMBA_PIN;
#ifdef NUCLEO
   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING; // Fake SMBA alert mapped to user button
   GPIO_InitStruct.Pull = GPIO_PULLDOWN;
#else
   //GPIO_InitStruct.Mode = GPIO_MODE_INPUT;    // TODO - Only on Marble v1.4
   GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING; // Roll-your-own SMB Alert for I2CPM (I2C3)
   GPIO_InitStruct.Pull = GPIO_PULLUP;
#endif
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   // TODO - Only on Marble v1.4
   // This is for LTM4673 PMBus Alert IRQ
   if (0) {
      HAL_NVIC_SetPriority(EXTI15_10_IRQn, 7, 7);
      HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
   }
   return;
}

int getI2CBusStatus(void) {
  return i2cBusStatus;
}

void resetI2CBusStatus(void) {
  i2cBusStatus = 0;
  return;
}

uint8_t fsynthGetAddr(void) {
  uint8_t data[6];
  int rval = eeprom_read_fsynth(data, 6);
  if (rval >= 0) {
    return FSYNTH_GET_ADDR(data);
  }
  return 0;
}

uint8_t fsynthGetConfig(void) {
  uint8_t data[6];
  int rval = eeprom_read_fsynth(data, 6);
  if (rval >= 0) {
    return FSYNTH_GET_CONFIG(data);
  }
  return 0;
}

uint32_t fsynthGetFreq(void) {
  uint8_t data[6];
  int rval = eeprom_read_fsynth(data, 6);
  int freq;
  if (rval >= 0) {
    freq = FSYNTH_GET_FREQ(data);
    return (uint32_t)freq;
  }
  return 0;
}

int get_hw_rnd(uint32_t *result) {
  HAL_StatusTypeDef rc;
  rc = HAL_RNG_GenerateRandomNumber(&hrng, result);
  /*
  printf("CR = 0x%08lX\r\n", hrng.Instance->CR);
  printf("SR = 0x%08lX\r\n", hrng.Instance->SR);
  printf("DR = 0x%08lX\r\n", hrng.Instance->DR);
  */
  return rc;
}

/* Enable MGT clock cross-point switch if 3.3V rail is ON
 * Returns 0 if power good.
 */
int mgtclk_xpoint_en(void)
{
  int rval=0;
   if ((marble_get_pcb_rev() <= Marble_v1_3) & xrp_ch_status(XRP7724, 1)) { // CH1: 3.3V
      printf("+ Init XRP7724 and adn4600\r\n");
      fflush(stdout);
      adn4600_init();
   } else if ((marble_get_pcb_rev() >= Marble_v1_4) & ltm4673_ch_status(LTM4673)) {
      printf("+ Init adn4600...\r\n");
      fflush(stdout);
      adn4600_init();
   } else {
      printf("+ Skipping adn4600 init\r\n");
      fflush(stdout);
      _pwr_good = 0;
      // This will trigger a board_init in the main loop if PWRGD is asserted on the first check
      _pwr_state = PWR_GOOD-1;
      rval = 1;
   }
   return rval;
}

static void show_mmc_ID(void) {
   printf("MMC CHIP ID: DEVID 0x%04X REVID 0x%04X\r\n", (uint16_t)(HAL_GetDEVID() & 0xffff), (uint16_t)(HAL_GetREVID() & 0xffff));
   return;
}

static void show_PHY_ID(void) {
    uint32_t id1, id2;
    HAL_ETH_ReadPHYRegister(&heth, MDIO_PHY_REG_PHY_ID_1, &id1);
    HAL_ETH_ReadPHYRegister(&heth, MDIO_PHY_REG_PHY_ID_2, &id2);
   printf("PHY ID: 0x%04lX 0x%04lX\r\n", id1, id2);
   return;
}

void marble_pmod_config_outputs(void) {
  //printf("marble_pmod_config_outputs\r\n");
  pmod_config_direction(GPIO_MODE_OUTPUT_PP);
  return;
}

void marble_pmod_config_inputs(void) {
  //printf("marble_pmod_config_inputs\r\n");
  pmod_config_direction(GPIO_MODE_INPUT);
  return;
}

static void pmod_config_direction(uint32_t direction) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Mode = direction;
  if (direction == GPIO_MODE_OUTPUT_PP) {
    GPIO_InitStruct.Pull = GPIO_NOPULL;
  } else {
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  }
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  // PB9, PB10, PB14, PB15
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14|GPIO_PIN_15;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  // PC2, PC3
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  // PD5, PD6
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  return;
}

#define PMOD_TIMER      (TIM2)
/* TIM2 hangs off APB1 bus
 * APBx timer clocks comes from:
 *  sysclk -> AHB prescaler -> APB prescaler -> APBx conditional doubler
 *  If APB prescaler = 1, then APBx doubler = 1x, else 2x
 *  FREQUENCY_APB1TIM
 *  ratio = PMOD_TIMER_RATIO
 *  PSC = (ratio >> 16) + 1
 *  ARR = ratio / PSC
 */

#define PMOD_TIMER_RATIO          (FREQUENCY_APB1TIM/FREQUENCY_PMOD_TIMER)

static void pmod_timer_interrupt_enable(void) {
  HAL_NVIC_SetPriority(TIM2_IRQn, 7, 7);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  return;
}

static void pmod_timer_interrupt_disable(void) {
  HAL_NVIC_DisableIRQ(TIM2_IRQn);
  return;
}

void marble_pmod_timer_enable(void) {
  //printf("Timer enable\r\n");
  pmod_timer_interrupt_enable();
  uint32_t cr1 = PMOD_TIMER->CR1;
  cr1 |= TIM_CR1_CEN;
  PMOD_TIMER->CR1 = cr1;
  return;
}

void marble_pmod_timer_disable(void) {
  //printf("Timer disable\r\n");
  uint32_t cr1 = PMOD_TIMER->CR1;
  cr1 &= ~TIM_CR1_CEN;
  PMOD_TIMER->CR1 = cr1;
  pmod_timer_interrupt_disable();
  return;
}

void marble_pmod_timer_config(void) {
  //PMOD_TIMER->CR1
  //PMOD_TIMER->CR2
  //PMOD_TIMER->SMCR
  //PMOD_TIMER->DIER
  //PMOD_TIMER->SR
  //PMOD_TIMER->EGR
  //PMOD_TIMER->CCMR1
  //PMOD_TIMER->CCMR2
  //PMOD_TIMER->CCER
  //PMOD_TIMER->CNT
  //PMOD_TIMER->PSC
  //PMOD_TIMER->ARR
  //PMOD_TIMER->RCR
  //PMOD_TIMER->CCR1
  //PMOD_TIMER->CCR2
  //PMOD_TIMER->CCR3
  //PMOD_TIMER->CCR4
  //PMOD_TIMER->BDTR
  //PMOD_TIMER->DCR
  //PMOD_TIMER->DMAR
  //PMOD_TIMER->OR
  __HAL_RCC_TIM2_CLK_ENABLE();
  // Set the prescaler and auto-reload register
  uint16_t scratch = (PMOD_TIMER_RATIO >> 16) + 1;
  PMOD_TIMER->PSC = (uint32_t)scratch;
  PMOD_TIMER->ARR = (uint32_t)(PMOD_TIMER_RATIO/scratch);
  //printf("Configuring TIM2 with PSC = %d, ARR = %d\r\n", scratch, PMOD_TIMER_RATIO/scratch);
  // Enable interrupt on underflow event (UEV)
  PMOD_TIMER->DIER = TIM_DIER_UIE;
  // Enable downcounter mode
  PMOD_TIMER->CR1 = TIM_CR1_DIR;
  return;
}

/*
Signal  Portbit J16 Pin
-----------------------
Pmod3_0 PB9     1
Pmod3_1 PC3     3
Pmod3_2 PC2     5
Pmod3_3 PB10    7
Pmod3_4 PB14    2
Pmod3_5 PB15    4
Pmod3_6 PD6     6
Pmod3_7 PD5     8
*/
/*                                        Pmod3_0     Pmod3_1     Pmod3_2     Pmod3_3      Pmod3_4      Pmod3_5      Pmod3_6     Pmod3_7  */
static GPIO_TypeDef *pmod_gpio_ports[] = {GPIOB,      GPIOC,      GPIOC,      GPIOB,       GPIOB,       GPIOB,       GPIOD,      GPIOD};
static uint16_t pmod_gpio_pins[] =       {GPIO_PIN_9, GPIO_PIN_3, GPIO_PIN_2, GPIO_PIN_10, GPIO_PIN_14, GPIO_PIN_15, GPIO_PIN_6, GPIO_PIN_5};

void marble_pmod_set_gpio(uint8_t pinnum, bool state) {
  if (pinnum > 7) return;
  HAL_GPIO_WritePin(pmod_gpio_ports[pinnum], pmod_gpio_pins[pinnum], state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return;
}

void TIM2_IRQHandler(void) {
  // Clear the update interrupt flag (ok, all interrupt flags)
  PMOD_TIMER->SR = 0;
  system_pmod_led_isr();
  return;
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */

#ifdef  USE_FULL_ASSERT
/**
   * @brief  Reports the name of the source file and the source line number
   *         where the assert_param error has occurred.
   * @param  file: pointer to the source file name
   * @param  line: assert_param error line source number
   * @retval None
*/
void assert_failed(uint8_t *file, uint32_t line)
{
   /* USER CODE BEGIN 6 */
   /* User can add his own implementation to report the file name and line number,
      tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
   /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */


/// @brief      Obtain the STM32 system reset cause
/// @param      None
/// @return     The system reset cause
static reset_cause_t reset_cause_get(void)
{
    printf("+ Retrieving last reset cause...\r\n");
    fflush(stdout);
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))
    {
        reset_cause = RESET_CAUSE_LOW_POWER_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
    {
        reset_cause = RESET_CAUSE_WINDOW_WATCHDOG_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    {
        reset_cause = RESET_CAUSE_INDEPENDENT_WATCHDOG_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
    {
        // This reset is induced by calling the ARM CMSIS
        // `NVIC_SystemReset()` function!
        reset_cause = RESET_CAUSE_SOFTWARE_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
    {
        reset_cause = RESET_CAUSE_POWER_ON_POWER_DOWN_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
    {
        reset_cause = RESET_CAUSE_EXTERNAL_RESET_PIN_RESET;
    }
    // Needs to come *after* checking the `RCC_FLAG_PORRST` flag in order to
    // ensure first that the reset cause is NOT a POR/PDR reset. See note
    // below.
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))
    {
        reset_cause = RESET_CAUSE_BROWNOUT_RESET;
    }
    else
    {
        reset_cause = RESET_CAUSE_UNKNOWN;
    }

    // Clear all the reset flags or else they will remain set during future
    // resets until system power is fully removed.
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return reset_cause;
}

// Note: any of the STM32 Hardware Abstraction Layer (HAL) Reset and Clock
// Controller (RCC) header files, such as
// "STM32Cube_FW_F7_V1.12.0/Drivers/STM32F7xx_HAL_Driver/Inc/stm32f7xx_hal_rcc.h",
// "STM32Cube_FW_F2_V1.7.0/Drivers/STM32F2xx_HAL_Driver/Inc/stm32f2xx_hal_rcc.h",
// etc., indicate that the brownout flag, `RCC_FLAG_BORRST`, will be set in
// the event of a "POR/PDR or BOR reset". This means that a Power-On Reset
// (POR), Power-Down Reset (PDR), OR Brownout Reset (BOR) will trip this flag.
// See the doxygen just above their definition for the
// `__HAL_RCC_GET_FLAG()` macro to see this:
//      "@arg RCC_FLAG_BORRST: POR/PDR or BOR reset." <== indicates the Brownout
//      Reset flag will *also* be set in the event of a POR/PDR.
// Therefore, you must check the Brownout Reset flag, `RCC_FLAG_BORRST`, *after*
// first checking the `RCC_FLAG_PORRST` flag in order to ensure first that the
// reset cause is NOT a POR/PDR reset.

/// @brief      Obtain the system reset cause as an ASCII-printable name string
///             from a reset cause type
/// @param[in]  reset_cause     The previously-obtained system reset cause
/// @return     A null-terminated ASCII name string describing the system
///             reset cause
static const char * reset_cause_get_name(void)
{
    const char * reset_cause_name = "TBD";

    switch (reset_cause)
    {
        case RESET_CAUSE_UNKNOWN:
            reset_cause_name = "UNKNOWN";
            break;
        case RESET_CAUSE_LOW_POWER_RESET:
            reset_cause_name = "LOW_POWER_RESET";
            break;
        case RESET_CAUSE_WINDOW_WATCHDOG_RESET:
            reset_cause_name = "WINDOW_WATCHDOG_RESET";
            break;
        case RESET_CAUSE_INDEPENDENT_WATCHDOG_RESET:
            reset_cause_name = "INDEPENDENT_WATCHDOG_RESET";
            break;
        case RESET_CAUSE_SOFTWARE_RESET:
            reset_cause_name = "SOFTWARE_RESET";
            break;
        case RESET_CAUSE_POWER_ON_POWER_DOWN_RESET:
            reset_cause_name = "POWER-ON_RESET (POR) / POWER-DOWN_RESET (PDR)";
            break;
        case RESET_CAUSE_EXTERNAL_RESET_PIN_RESET:
            reset_cause_name = "EXTERNAL_RESET_PIN_RESET";
            break;
        case RESET_CAUSE_BROWNOUT_RESET:
            reset_cause_name = "BROWNOUT_RESET (BOR)";
            break;
    }

    return reset_cause_name;
}

void print_reset_cause(void)
{
    printf("Last MMC reset cause is \"%s\"\r\n",
           reset_cause_get_name());
    return;
}


static void print_time(uint32_t total_seconds){
    uint32_t days    = total_seconds / 86400;
    uint32_t hours   = (total_seconds % 86400) / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;
    // uint32_t ms_remainder  = total_ms % 1000;
    printf(
        "%lu days, %02lu:%02lu:%02lu",
        (unsigned long) days,
        (unsigned long) hours,
        (unsigned long) minutes,
        (unsigned long) seconds
        // (unsigned long) ms_remainder
    );
}

static void print_uptime(void) //wraps around at ~136 years
{
    uint32_t uptime_ms = marble_get_tick();  // current tick in ms (uint32_t)
    uint64_t total_ms = (uint64_t)tick_overflow_count * (uint64_t)UINT32_MAX
                      + (uint64_t)uptime_ms;
    uint32_t total_seconds = total_ms / 1000;
    printf("Uptime: ");
    print_time(total_seconds);
    printf("\r\n");
}
