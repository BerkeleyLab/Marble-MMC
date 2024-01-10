// eth_dev.c
// Ethernet device responder
// Supports ARP, ICMP, and UDP
// Stuck with STM32_HAL API for now

// ==== NOTE! Only enabled on nucleo target
#ifdef NUCLEO

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stm32f2xx_hal.h"
#include "marble_api.h"
#include "console.h"
#include "st-eeprom.h"
#include "lass.h"

#define OFFLOAD_CHECKSUM
typedef enum {
  PKT_TYPE_UNK,
  PKT_TYPE_ARP,
  PKT_TYPE_ICMP,
  PKT_TYPE_UDP
} pkt_type_t;
static pkt_type_t eth_pkt_type(uint8_t *pbuf, uint32_t len);
static uint32_t eth_respond_arp(uint8_t *pbuf, uint32_t len);
static uint32_t eth_respond_icmp(uint8_t *pbuf, uint32_t len);
static uint32_t eth_respond_udp(uint8_t *pbuf, uint32_t len);
static int eth_send_response(uint8_t *pbuf, uint32_t len);
uint8_t OWN_MAC[MAC_LENGTH] = {0xce, 0xce, 0x03, 0x27, 0x04, 0x06};
uint8_t OWN_IP[IP_LENGTH] = {192,168,51,50};
static uint16_t lass_port = 50006;

// marble_board.c
extern ETH_HandleTypeDef heth;

void eth_dev_init(uint16_t port) {
  lass_port = port;
  mac_ip_data_t pdata;
  int rval = eeprom_read_ip_addr(pdata.ip, IP_LENGTH);
  if (rval == 0) {
    memcpy(OWN_IP, pdata.ip, IP_LENGTH);
  }
  rval = eeprom_read_mac_addr(pdata.mac, MAC_LENGTH);
  if (rval == 0) {
    memcpy(OWN_MAC, pdata.mac, MAC_LENGTH);
  }
  return;
}

void eth_handle_packet(void) {
  HAL_StatusTypeDef rval;
  if ((rval = HAL_ETH_GetReceivedFrame_IT(&heth)) != HAL_OK) {
    printf("Error: %d\r\n", rval);
  }
  uint32_t len = heth.RxFrameInfos.length;
  /*
  ETH_DMADescTypeDef *FSRxDesc;          //!< First Segment Rx Desc
  ETH_DMADescTypeDef *LSRxDesc;          //!< Last Segment Rx Desc
  uint32_t SegCount;                    //!< Segment count
  uint32_t length;                       //!< Frame length
  uint32_t buffer;                       //!< Frame buffer
  */

  //uint8_t *buf = (uint8_t *)heth.RxFrameInfos.buffer;
  ETH_DMADescTypeDef *dmarxdesc = heth.RxFrameInfos.FSRxDesc;
  printf("Eth packet. length %ld, SegCount %ld\r\n", len, heth.RxFrameInfos.SegCount);
  uint8_t *buf = (uint8_t *)heth.RxFrameInfos.buffer;
#if 0
  uint8_t *buf;
  for (uint32_t nbuf = 0; nbuf < ETH_RXBUFNB; nbuf++) {
    buf = eth_rx_buf[nbuf];
    printf("Buf[%ld]: ", nbuf);
    for (int nbyte = 0; nbyte < 16; nbyte++) {
      printf("0x%02x ", buf[nbyte]);
    }
    printf("\r\n");
  }
#endif
  for (uint32_t nbyte = 0; nbyte < len; nbyte++) {
    printf("0x%02x ", buf[nbyte]);
  }
  printf("\r\n");

  uint32_t handled = 0;
  uint8_t *pktbuf = buf;
  while (len > 0) {
    // Handle various protocols
    pkt_type_t pkt_type = eth_pkt_type(pktbuf, len);
    switch (pkt_type) {
      case PKT_TYPE_ARP:
        handled = eth_respond_arp(pktbuf, len);
        break;
      case PKT_TYPE_ICMP:
        handled = eth_respond_icmp(pktbuf, len);
        break;
      case PKT_TYPE_UDP:
        handled = eth_respond_udp(pktbuf, len);
        break;
      default:
        printf("Unknown packet type\r\n");
        len = 0;
        break;
    }
    pktbuf += handled;
    len -= handled;
    if ((uint32_t)(pktbuf-buf) > ETH_MAX_PACKET_SIZE) {
      // Get the next descriptor
      dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
      buf = (uint8_t *)(dmarxdesc->Buffer1Addr);
      pktbuf = buf;
    }
  }

  /* Release descriptors to DMA */
  /* Point to first descriptor */
  dmarxdesc = heth.RxFrameInfos.FSRxDesc;
  /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
  for (uint32_t i=0; i< heth.RxFrameInfos.SegCount; i++) {
    dmarxdesc->Status |= ETH_DMARXDESC_OWN;
    dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
  }

  /* Clear Segment_Count */
  heth.RxFrameInfos.SegCount =0;

  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET) {
    /* Clear RBUS ETHERNET DMA flag */
    heth.Instance->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    heth.Instance->DMARPDR = 0;
  }
}

#define OFFSET_ETHERTYPE    (12)
#define OFFSET_ARP_PTYPE    (16)
#define OFFSET_ARP_OPER     (20)
#define OFFSET_ARP_SHA      (22)
#define OFFSET_ARP_SPA      (28)
#define OFFSET_ARP_THA      (32)
#define OFFSET_ARP_TPA      (38)

#define OFFSET_IP_VERSION_IHL (14)
#define OFFSET_IP_DSCP_ECN   (15)
#define OFFSET_IP_TOTAL_LENGTH (16)
#define OFFSET_IP_ID         (18)
#define OFFSET_IP_FLAG_FRAG  (20)
#define OFFSET_IP_TTL        (22)
#define OFFSET_IP_PROTOCOL   (23)
#define OFFSET_IP_CHECKSUM   (24)
#define OFFSET_IP_SRC_IP     (26)
#define OFFSET_IP_DEST_IP    (30)

#define OFFSET_IP_PAYLOAD_UDP_SRC_PORT (0)
#define OFFSET_IP_PAYLOAD_UDP_DEST_PORT (2)
#define OFFSET_IP_PAYLOAD_UDP_LENGTH    (4)
#define OFFSET_IP_PAYLOAD_UDP_CHECKSUM  (6)
#define OFFSET_IP_PAYLOAD_UDP_DATA      (8)

#define IP_PROTOCOL_IP      (0)
#define IP_PROTOCOL_ICMP    (1)
#define IP_PROTOCOL_TCP     (6)
#define IP_PROTOCOL_UDP     (17)

#define PAYLOAD_OFFSET_ICMP_TYPE (0)
#define PAYLOAD_OFFSET_ICMP_CODE (1)
#define PAYLOAD_OFFSET_ICMP_CHECKSUM (2)
#define PAYLOAD_OFFSET_ICMP_REST_OF_HEADER (4)

static pkt_type_t eth_pkt_type(uint8_t *pbuf, uint32_t len) {
  if (len < 60) {
    // TODO Figure out what we really think is the min packet size
    return PKT_TYPE_UNK;
  }
  pkt_type_t type = PKT_TYPE_UNK;
  // Check ethertype
  uint16_t ethertype = *((uint16_t *)&pbuf[OFFSET_ETHERTYPE]);
  uint16_t gp16;
  uint8_t ihl;
  switch (ethertype) {
    case 0x0608:  // ARP (big-endian)
      printf("ARP\r\n");
      // If PTYPE != 0x0800 (0x0008 big-endian), don't respond. Only responding to IPv4
      gp16 = *((uint16_t *)&pbuf[OFFSET_ARP_PTYPE]);
      if (gp16 != 0x0008) {
        printf("Not IPv4: gp16 = 0x%x\r\n", gp16);
        break;
      }
      // If ARP OPER != 0x0001 (0x0100 big-endian), don't respond
      gp16 = *((uint16_t *)&pbuf[OFFSET_ARP_OPER]);
      if (gp16 != 0x0100) {
        printf("Not ARP request: gp16 = 0x%x\r\n", gp16);
        break;
      }
      // Ok, we'll respond
      type = PKT_TYPE_ARP;
      break;
    case 0x0008:  // IPv4 (big-endian)
      printf("IPv4\r\n");
      // Check version (must be 4)
      if ((pbuf[OFFSET_IP_VERSION_IHL] & 0xf0) != 0x40) {
        break;
      }
      // Read header len
      ihl = pbuf[OFFSET_IP_VERSION_IHL] & 0xf; // actually just IHL
      if ((ihl < 5) || (ihl > 15)) {
        printf("IHL invalid (%d)\r\n", ihl);
        break;
      }
      // Protocol
      if (pbuf[OFFSET_IP_PROTOCOL] == IP_PROTOCOL_ICMP) {
        type = PKT_TYPE_ICMP;
      } else if (pbuf[OFFSET_IP_PROTOCOL] == IP_PROTOCOL_UDP) {
        type = PKT_TYPE_UDP;
      } else {
        printf("Unknown IP protocol %d\r\n", pbuf[OFFSET_IP_PROTOCOL]);
        break;
      }
      break;
    case 0x4208:  // WakeOnLAN (big-endian)
      printf("WakeOnLAN\r\n");
      break;
    case 0xDD86:  // IPv6 (big-endian)
      printf("IPv6\r\n");
      break;
    default:
      printf("Ethertype = 0x%04x\r\n", ethertype);
      break;
  }
  return type;
}

static uint32_t eth_respond_arp(uint8_t *pbuf, uint32_t len) {
  // Use src MAC as dest MAC in ethernet header
  memcpy(pbuf, pbuf+6, 6);    // Clobber dest MAC with src MAC
  memcpy(pbuf+6, OWN_MAC, 6); // Clobber src MAC with own MAC
  // Switch ARP OPER from 1 to 2
  pbuf[OFFSET_ARP_OPER+1] = 2;
  // Use SHA as THA
  memcpy(pbuf+OFFSET_ARP_THA, pbuf+OFFSET_ARP_SHA, 6);    // Clobber THA with SHA
  // Add own SHA
  memcpy(pbuf+OFFSET_ARP_SHA, OWN_MAC, 6);
  // Use SPA as TPA
  memcpy(pbuf+OFFSET_ARP_TPA, pbuf+OFFSET_ARP_SPA, 4);    // Clobber TPA with SPA
  // Add own SPA
  memcpy(pbuf+OFFSET_ARP_SPA, OWN_IP, 4);
  // Send response  TODO - What actual length should we respond with?
  eth_send_response(pbuf, len);
  return len;
}

static uint32_t eth_respond_icmp(uint8_t *pbuf, uint32_t len) {
  uint8_t header_len = 4*(pbuf[OFFSET_IP_VERSION_IHL] & 0xf); // header_len = 4*IHL
  // Total len
  uint16_t total_len = ((uint16_t)pbuf[OFFSET_IP_TOTAL_LENGTH] << 8) | (uint16_t)pbuf[OFFSET_IP_TOTAL_LENGTH+1];
  if (total_len < 20) {
    printf("Invalid total_len (%d)\r\n", total_len);
    return len;
  }
  // Use src MAC as dest MAC in ethernet header
  memcpy(pbuf, pbuf+6, 6);    // Clobber dest MAC with src MAC
  memcpy(pbuf+6, OWN_MAC, 6); // Clobber src MAC with own MAC
  // Use Source IP as Dest IP
  memcpy(pbuf+OFFSET_IP_DEST_IP, pbuf+OFFSET_IP_SRC_IP, 4);
  // Add own IP as Source IP
  memcpy(pbuf+OFFSET_IP_SRC_IP, OWN_IP, 4);
  // IPv4 payload (ICMP)
  // ICMP type code
  if (!((pbuf[14+header_len+PAYLOAD_OFFSET_ICMP_TYPE] == 8) && (pbuf[14+header_len+PAYLOAD_OFFSET_ICMP_CODE] == 0))) {
    printf("Not an ICMP request: %d %d\r\n", pbuf[header_len+PAYLOAD_OFFSET_ICMP_TYPE], pbuf[header_len+PAYLOAD_OFFSET_ICMP_CODE]);
    return len;
  }
  // ICMP reply
  pbuf[14+header_len+PAYLOAD_OFFSET_ICMP_TYPE] = 0;
  // Use the STM32 auto-checksum-inserter feature (requires the checksum field be zero'd)
  memset(&pbuf[OFFSET_IP_CHECKSUM], 0, 2);
  memset(&pbuf[14+header_len+PAYLOAD_OFFSET_ICMP_CHECKSUM], 0, 2);
#ifndef OFFLOAD_CHECKSUM
  // TODO Calculate the header checksum in software
#endif
  // Send response  TODO - What actual length should we respond with?
  eth_send_response(pbuf, 14+total_len);
  return (uint32_t)(14+total_len);
}

static uint32_t eth_respond_udp(uint8_t *pbuf, uint32_t len) {
  uint8_t header_len = 4*(pbuf[OFFSET_IP_VERSION_IHL] & 0xf); // header_len = 4*IHL
  // First check the port
  uint16_t gp16 = ((uint16_t)pbuf[14+header_len+OFFSET_IP_PAYLOAD_UDP_DEST_PORT] << 8) \
                  | (uint16_t)pbuf[14+header_len+OFFSET_IP_PAYLOAD_UDP_DEST_PORT+1];
  if (gp16 != lass_port) {
    // Don't respond
    printf("Not our port: %d\r\n", gp16);
    return len;
  }
  // Total len
  gp16 = ((uint16_t)pbuf[OFFSET_IP_TOTAL_LENGTH] << 8) | (uint16_t)pbuf[OFFSET_IP_TOTAL_LENGTH+1];
  if (gp16 < 20) {
    printf("Invalid total_len (%d)\r\n", gp16);
    return len;
  }
  // Payload: LASS protocol
  uint8_t *prsp;
  int rsp_size = 0;
  int do_reply = parse_lass((void *)&pbuf[14+header_len+OFFSET_IP_PAYLOAD_UDP_DATA], len, \
                            (volatile uint8_t **)&prsp, (volatile int *)&rsp_size);
  if (do_reply == 0) {
    printf("UDP payload not LASS\r\n");
    return len;
  }
  // Use src MAC as dest MAC in ethernet header
  memcpy(pbuf, pbuf+6, 6);    // Clobber dest MAC with src MAC
  memcpy(pbuf+6, OWN_MAC, 6); // Clobber src MAC with own MAC
  // Use Source IP as Dest IP
  memcpy(pbuf+OFFSET_IP_DEST_IP, pbuf+OFFSET_IP_SRC_IP, 4);
  // Add own IP as Source IP
  memcpy(pbuf+OFFSET_IP_SRC_IP, OWN_IP, 4);
  // Use the STM32 auto-checksum-inserter feature (requires the checksum field be zero'd)
  memset(&pbuf[OFFSET_IP_CHECKSUM], 0, 2);
  // IPv4 payload (UDP)
  // Use SRC port as DEST port
  memcpy(pbuf+14+header_len+OFFSET_IP_PAYLOAD_UDP_DEST_PORT, pbuf+14+header_len+OFFSET_IP_PAYLOAD_UDP_SRC_PORT, 2);
  // Use lass_port as SRC port
  pbuf[14+header_len+OFFSET_IP_PAYLOAD_UDP_SRC_PORT] = (uint8_t)((lass_port >> 8) & 0xff);
  pbuf[14+header_len+OFFSET_IP_PAYLOAD_UDP_SRC_PORT+1] = (uint8_t)(lass_port & 0xff);
  // Zero-out the UDP checksum to let the STM32 do its thing
  memset(&pbuf[14+header_len+OFFSET_IP_PAYLOAD_UDP_CHECKSUM], 0, 2);
  eth_send_response(prsp, rsp_size);
  return 14+gp16;
}

#define NO_ERROR    (0)
#define ERR_NO_BUFFER (-1)
#define ERR_TOO_BIG (-2)
static int eth_send_response(uint8_t *pbuf, uint32_t len) {
  uint8_t *buffer = (uint8_t *)(heth.TxDesc->Buffer1Addr);
  ETH_DMADescTypeDef *DmaTxDesc  = heth.TxDesc;
  int error = NO_ERROR;

  // copy frame from pbuf to driver buffers
  // Is this buffer available? If not, goto error
  if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
  {
    printf("Buffer not available\r\n");
    error = ERR_NO_BUFFER;
  }

  // Check if the length of data to copy is bigger than Tx buffer size
  if ( len > (uint32_t)ETH_TX_BUF_SIZE ) {
    printf("Too big (%ld > %ld)\r\n", len, (uint32_t)ETH_TX_BUF_SIZE);
    error = ERR_TOO_BIG;
  }

  if (error == NO_ERROR) {
    // Copy data to Tx buffer
    memcpy(buffer, pbuf, len);

    // Prepare transmit descriptors to give to DMA
    printf("Sending frame of len %ld... ", len);
    HAL_StatusTypeDef rval = HAL_ETH_TransmitFrame(&heth, len);
    printf("rval = %d\r\n", rval);
#if 1
    for (uint32_t nbyte = 0; nbyte < len; nbyte++) {
      printf("0x%02x ", buffer[nbyte]);
    }
    printf("\r\n");
  }
#endif

  // When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission
  if ((heth.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET)
  {
    // Clear TUS ETHERNET DMA flag
    heth.Instance->DMASR = ETH_DMASR_TUS;

    // Resume DMA transmission
    heth.Instance->DMATPDR = 0;
  }

  return error;
}
#endif // ifdef NUCLEO
