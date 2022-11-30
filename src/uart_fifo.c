/*  File: uart_fifo.c
 *  Author: Auto-generated by mkqueue.py
 */

#include "uart_fifo.h"

// ============================= Private Typedefs ==============================
typedef struct {
  uint8_t pIn;
  uint8_t pOut;
  uint8_t full;
  uint8_t queue[UART_QUEUE_ITEMS];
} UART_queue_t;

typedef struct {
  uint32_t pIn;
  uint32_t pOut;
  uint8_t full;
  uint8_t queue[UARTTX_QUEUE_ITEMS];
} UARTTX_queue_t;

// ============================= Private Variables =============================
static UART_queue_t UART_queue;
static UARTTX_queue_t UARTTX_queue;
static uint8_t _dataLost = UART_DATA_NOT_LOST;

// =========================== Function Definitions ============================
void UARTQUEUE_Init(void) {
  UART_queue.pIn = 0;
  UART_queue.pOut = 0;
  UART_queue.full = 0;
  _dataLost = 0;
  UARTTX_queue.pIn = 0;
  UARTTX_queue.pOut = 0;
  UARTTX_queue.full = 0;
  return;
}

void UARTQUEUE_Clear(void) {
  UART_queue.pIn = 0;
  UART_queue.pOut = 0;
  UART_queue.full = 0;
  return;
}

uint8_t UARTQUEUE_Add(uint8_t *item) {
  // If full, return error
  if (UART_queue.full) {
    return UART_QUEUE_FULL;
  }
  // Copy item into queue
  for (unsigned int n = 0; n < sizeof(uint8_t); n++) {
    *((uint8_t *)&(UART_queue.queue[UART_queue.pIn]) + n) = *((uint8_t *)item + n);
  }
  // Wrap pIn at boundary
  if (UART_queue.pIn == UART_QUEUE_ITEMS - 1) {
    UART_queue.pIn = 0;
  } else {
    UART_queue.pIn++;
  }
  // Check for full condition
  if (UART_queue.pIn == UART_queue.pOut) {
    UART_queue.full = 1;
  }
  return UART_QUEUE_OK;
}

uint8_t UARTQUEUE_Get(volatile uint8_t *item) {
  // Check for empty queue
  if ((UART_queue.pIn == UART_queue.pOut) && (UART_queue.full == 0)) {
    return UART_QUEUE_EMPTY;
  }
  // Copy next data from the queue to item
  for (unsigned int n = 0; n < sizeof(uint8_t); n++) {
    *((uint8_t *)item + n) = *((uint8_t *)&(UART_queue.queue[UART_queue.pOut]) + n);
  }
  // Wrap pOut at boundary
  if (UART_queue.pOut == UART_QUEUE_ITEMS - 1) {
    UART_queue.pOut = 0;
  } else {
    UART_queue.pOut++;
  }
  // Clear full condition
  UART_queue.full = 0;
  return UART_QUEUE_OK;
}

uint8_t UARTQUEUE_Status(void) {
  if ((UART_queue.pIn == UART_queue.pOut) && (UART_queue.full == 0)) {
    return UART_QUEUE_EMPTY;
  }
  if (UART_queue.full) {
    return UART_QUEUE_FULL;
  }
  // If not full or empty, it is non-empty (at least one item in queue)
  return UART_QUEUE_OK;
}

/*
 * int UARTQUEUE_ShiftOut(uint8_t *pData, int len);
 *  Shift up to 'len'
 */
int UARTQUEUE_ShiftOut(uint8_t *pData, int len) {
  int nShifted = 0;
  uint8_t dataOut;
  while (UARTQUEUE_Get(&dataOut) != UART_QUEUE_EMPTY) {
    *(pData++) = dataOut;
    if (++nShifted == len) {
      break;
    }
  }
  return nShifted;
}

void UARTQUEUE_SetDataLost(uint8_t lost) {
  if (lost) {
    _dataLost = UART_DATA_LOST;
  }
  else {
    _dataLost = UART_DATA_NOT_LOST;
  }
  return;
}

uint8_t UARTQUEUE_IsDataLost(void) {
  return _dataLost;
}

uint8_t UARTTXQUEUE_Add(uint8_t *item) {
  // If full, return error
  if (UARTTX_queue.full) {
    return UARTTX_QUEUE_FULL;
  }
  // Copy item into queue
  for (unsigned int n = 0; n < sizeof(uint8_t); n++) {
    *((uint8_t *)&(UARTTX_queue.queue[UARTTX_queue.pIn]) + n) = *((uint8_t *)item + n);
  }
  // Wrap pIn at boundary
  if (UARTTX_queue.pIn == UARTTX_QUEUE_ITEMS - 1) {
    UARTTX_queue.pIn = 0;
  } else {
    UARTTX_queue.pIn++;
  }
  // Check for full condition
  if (UARTTX_queue.pIn == UARTTX_queue.pOut) {
    UARTTX_queue.full = 1;
  }
  return UARTTX_QUEUE_OK;
}

uint8_t UARTTXQUEUE_Get(volatile uint8_t *item) {
  // Check for empty queue
  if ((UARTTX_queue.pIn == UARTTX_queue.pOut) && (UARTTX_queue.full == 0)) {
    return UARTTX_QUEUE_EMPTY;
  }
  // Copy next data from the queue to item
  for (unsigned int n = 0; n < sizeof(uint8_t); n++) {
    *((uint8_t *)item + n) = *((uint8_t *)&(UARTTX_queue.queue[UARTTX_queue.pOut]) + n);
  }
  // Wrap pOut at boundary
  if (UARTTX_queue.pOut == UARTTX_QUEUE_ITEMS - 1) {
    UARTTX_queue.pOut = 0;
  } else {
    UARTTX_queue.pOut++;
  }
  // Clear full condition
  UARTTX_queue.full = 0;
  return UARTTX_QUEUE_OK;
}

uint8_t UARTTXQUEUE_Status(void) {
  if ((UARTTX_queue.pIn == UARTTX_queue.pOut) && (UARTTX_queue.full == 0)) {
    return UARTTX_QUEUE_EMPTY;
  }
  if (UARTTX_queue.full) {
    return UARTTX_QUEUE_FULL;
  }
  // If not full or empty, it is non-empty (at least one item in queue)
  return UARTTX_QUEUE_OK;
}

// ========================== Non- Blocking API ===============================

/*
 * int USART_Tx_LL_Queue(char *msg, int len);
 *    Returns number of bytes added to queue
 *    Returns -1 on full
 */
int USART_Tx_LL_Queue(char *msg, int len) {
  // Add to byte queue
  uint8_t rval;
  int n = 0;
  for (n = 0; n < len; n++) {
    rval = UARTTXQUEUE_Add((uint8_t *)(msg + n)); // Type uint8_t* (not char*)
    if (rval == UARTTX_QUEUE_FULL) {
      return -1;
    }
  }
  return n;
}

/*
 * int USART_Rx_LL_Queue(volatile char *msg, int len);
 *    Attempt to read 'len' characters from Rx queue into 'msg'
 *    Returns number of chars read from queue.
 */
int USART_Rx_LL_Queue(volatile char *msg, int len) {
  int n = 0;
  while (UARTQUEUE_Get((uint8_t *)(msg + n)) != UART_QUEUE_EMPTY) {
    if (n++ >= len) {
      break;
    }
  }
  return n;
}
