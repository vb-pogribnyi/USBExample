#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "main.h"
#include <string.h>

/*
 * Make sure to allocate FIFOs and start the peripheral in main.c
 *
 *   // Set FIFO number to activate,
 *   // along with its size in 4-byte WORDS, e.g.
 *   // 0x20 = 32 words = 128 bytes
 *   HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x20);
 *   HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x20);
 *   HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2, 0x20);
 *   HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 3, 0x100);
 *   HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x40);
 *   HAL_PCD_Start(&hpcd_USB_OTG_FS);
 *
 */

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern uint16_t xfer_buff[];	// Data to transmit to the host. The data itself is generated elsewhere.
static uint8_t usb_buff[4];
static int is_ctrl_receive_pending = 0;
static const uint8_t int_packet_size = 48;		// Up to 64
static const uint8_t blk_packet_size = 64;		// Up to 64
static const uint16_t iso_packet_size = 280;	// Up to 1024
int is_xfer_requested = 0;

uint8_t device_descriptor[] = {
		0x12,		// Length
		0x01,		// Descriptor type
		0x00, 0x02,	// USB version
		0x00,		// Device class
		0x00,		// Device subclass
		0x00,		// Device protocol
		0x40,		// Max Packet Size (endpoint 0)
		0x83, 0x04,	// idVendor
		0x55, 0x12,	// idProduct
		0x01, 0x01,	// Device version
		0x00,		// iManufacturer
		0x00,		// iProduct
		0x00,		// iSerialNumber
		0x01		// Num configurations
};

uint8_t configuration_descriptor[] = {
		0x09,		// Length
		0x02, 		// Descriptor type
		0x00, 0x00, // Total length, to be filled later
		0x01,		// Num interfaces
		0x01,		// Configuration number
		0x00,		// iConfiguration
		0xc0,		// Attributes. SELF-POWERED, NO-REMOTE-WAKEUP
		0x19,		// Max power. 50 mA
		// Interface descriptor
		0x09,		// Length
		0x04,		// Descriptor type
		0x00,		// Interface number
		0x00, 		// Alternate setting

		//------------ UPDATE IF ENDPOINTS UPDATED ---------------------------
		0x03,		// Endpoints number
		//--------------------------------------------------------------------

		0xff,		// Interface class. Custom
		0xff,		// Interface Subclass. Custom
		0xff,		// Interface Protocol. Custom
		0x00,		// iInterface
		// Interrupt endpoint descriptor
		0x07,		// Length
		0x05,		// Descriptor type
		0x01,		// Address. OUT 1
		0x03,		// Type. Interrupt
		int_packet_size, 0x00,		// Max packet size
		0x03,
		// Bulk endpoint descriptor
		0x07,		// Length
		0x05,		// Descriptor type
		0x82,		// Address. IN 2
		0x02,		// Type. Bulk
		blk_packet_size, 0x00,		// Max packet size
		0x03,
		// Isochronous endpoint descriptor
		0x07,		// Length
		0x05,		// Descriptor type
		0x83,		// Address. IN 3
		0x0D,		// Type. Iso, synchronous with SOF
		iso_packet_size & 0xff, (iso_packet_size & 0xff00) >> 8,		// Max packet size
		0x01,		// Interval. 1, request data every frame
};

// Control requests used for enumeration
#define STANDARD 0x80
#define GET_DESCRIPTOR 6
#define DESCRIPTOR_DEVICE 1
#define DESCRIPTOR_DEVICE_QUALIFIER 6
#define DESCRIPTOR_CONFIGURATION 2
#define SET_CONFIGURATION 9
#define SET_ADDRESS 5

// Custom control requests
#define CLASS_INPUT 0xC0
#define CLASS_OUTPUT 0x40


void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
	printf("In Reset handler\n");
	// Open OUT endpoint 0
	HAL_PCD_EP_Flush(&hpcd_USB_OTG_FS, 0x00);
	HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, 0x00, 64, 0);

	// Open IN endpoint 0
	HAL_PCD_EP_Flush(&hpcd_USB_OTG_FS, 0x80);
	HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, 0x80, 64, 0);
	((uint16_t*)configuration_descriptor)[1] = sizeof(configuration_descriptor);
}


// Handles enumeration process, reacts to custom control requests
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
	printf("Setup stage\n");
	for (int i = 0; i < 8; i++) {
		printf("0x%02X ", ((uint8_t*)hpcd->Setup)[i]);
	}
	printf("\n");

	uint8_t request_type = ((uint8_t*)hpcd->Setup)[0];
	uint8_t request = ((uint8_t*)hpcd->Setup)[1];
	uint8_t data1 = ((uint8_t*)hpcd->Setup)[2];
	uint8_t data2 = ((uint8_t*)hpcd->Setup)[3];
	uint8_t requested_length = ((uint16_t*)hpcd->Setup)[3];

	if (request_type == STANDARD && request == GET_DESCRIPTOR && data2 == DESCRIPTOR_DEVICE) {
		HAL_PCD_EP_Transmit(hpcd, 0x00, device_descriptor, sizeof(device_descriptor));
	} else if (request_type == STANDARD && request == GET_DESCRIPTOR && data2 == DESCRIPTOR_DEVICE_QUALIFIER) {
		printf("Sending configuration descriptor\n");
		HAL_PCD_EP_Transmit(hpcd, 0x00, 0, 0);
	} else if (request_type == STANDARD && request == GET_DESCRIPTOR && data2 == DESCRIPTOR_CONFIGURATION) {
		printf("Ignoring qualifier descriptor\n");

		if (requested_length > sizeof(configuration_descriptor)) requested_length = sizeof(configuration_descriptor);

		HAL_PCD_EP_Transmit(hpcd, 0x00, configuration_descriptor, requested_length);
	} else if (request == SET_ADDRESS) {
		printf("Setting address: %i\n", data1);
		HAL_PCD_SetAddress(hpcd, data1);
		HAL_PCD_EP_Transmit(hpcd, 0, 0, 0);
	} else if (request == SET_CONFIGURATION) {
		printf("Setting configuration, %i\n", data1);

		HAL_PCD_EP_Flush(&hpcd_USB_OTG_FS, 0x01);
		HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, 0x01, int_packet_size, EP_TYPE_INTR);
		HAL_PCD_EP_Flush(&hpcd_USB_OTG_FS, 0x82);
		HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, 0x82, blk_packet_size, EP_TYPE_BULK);
		HAL_PCD_EP_Flush(&hpcd_USB_OTG_FS, 0x83);
		HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, 0x83, iso_packet_size, EP_TYPE_ISOC);

		HAL_PCD_EP_Transmit(hpcd, 0, 0, 0);
	}
	if (request_type == CLASS_INPUT) {
		printf("Control IN request\n");
		sprintf((char*)usb_buff, "Hi!\n");
		if (requested_length > strlen((char*)usb_buff)) requested_length = strlen((char*)usb_buff);

		HAL_PCD_EP_Transmit(hpcd, 0, usb_buff, requested_length);
	} else if (request_type == CLASS_OUTPUT) {
		is_ctrl_receive_pending = 1;
		HAL_PCD_EP_Receive(hpcd, 0, usb_buff, requested_length);
		printf("Control OUT request with value %i\n", data1);
		HAL_PCD_EP_Transmit(hpcd, 0, 0, 0);
	}

}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
	printf("Data OUT stage, ep %i\n", epnum);
	if (is_ctrl_receive_pending) {
		is_ctrl_receive_pending = 0;
		printf("Received CTRL data: %s", usb_buff);
	}
}
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
	printf("Data IN stage, ep %i\n", epnum);
	if (epnum == 0) {
		HAL_PCD_EP_Receive(hpcd, 0x00, 0, 0);
	}
	else if (epnum == 2) {
		printf("INT data IN callback\n");
	}
	else if (epnum == 2) {
		printf("BULK data IN callback\n");
	}
	else if (epnum == 3) {
		printf("ISO data IN callback\n");
	}

}
