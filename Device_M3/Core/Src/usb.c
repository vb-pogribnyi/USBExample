#include "stm32f1xx_hal.h"
#include "main.h"
#include <stdio.h>
#include "string.h"
#include "usb.h"

extern PCD_HandleTypeDef hpcd_USB_FS;

uint8_t device_descriptor[] = {
		0x12,		// Length
		0x01,		// Descriptor type
		0x00, 0x02,	// USB version
		0x00,		// Device class
		0x00,		// Device subclass
		0x00,		// Device protocol
		0x40,		// Max Packet Size
		0x77, 0x04,	// idVendor
		0x22, 0x11,	// idProduct
		0x01, 0x01,	// Device version
		0x01,		// iManufacturer
		0x02,		// iProduct
		0x03,		// iSerialNumber
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
		0x01,		// Endpoints number
		0xff,		// Interface class. Custom
		0xff,		// Interface Subclass. Custom
		0xff,		// Interface Protocol. Custom
		0x04,		// iInterface
		// Bulk endpoint descriptor
		0x07,		// Length
		0x05,		// Descriptor type
		0x81,		// Address. IN 1
		0x02,		// Type. Bulk
		0x40, 0x00,		// Max packet size
		0x03,
};

uint8_t lang_string_descriptor[] = {
		0x08,		// Length
		0x03,		// Descriptor type
		0x09,		// en_US
		0x04,
//		0x22,		// uk_UA
//		0x04
};
uint8_t string_descriptor[255] = {
		0x00,		// Length (to be filled later)
		0x03 		// Descriptor type
};

uint8_t is_half1_available = 0;
uint8_t is_half2_available = 0;
uint8_t is_overrun = 0;
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (is_half1_available) is_overrun = 1;
	is_half1_available = 0;
	is_half2_available = 1;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (is_half2_available) is_overrun = 1;
	is_half1_available = 1;
	is_half2_available = 0;
}
extern uint16_t sound[SOUND_ITEMS];

#define STANDARD 0x80
#define STANDARD_HTD 0x00
#define GET_DESCRIPTOR 6
#define DESCRIPTOR_DEVICE 1
#define DESCRIPTOR_STRING 3
#define DESCRIPTOR_DEVICE_QUALIFIER 6
#define DESCRIPTOR_CONFIGURATION 2
#define SET_CONFIGURATION 9
#define SET_ADDRESS 5

#define MY_CUSTOM_IN_REQUEST_TYPE 0xCC
#define MY_CUSTOM_OUT_REQUEST_TYPE 0x4C
#define MY_CUSTOM_REQUEST 0x25


void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
	printf("In Reset handler\n");
	// Open OUT endpoint 0
	HAL_PCD_EP_Flush(hpcd, 0x00);
	HAL_PCD_EP_Open(hpcd, 0x00, 64, 0);

//	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, RESET);

	// Open IN endpoint 0
	HAL_PCD_EP_Flush(hpcd, 0x80);
	HAL_PCD_EP_Open(hpcd, 0x80, 64, 0);
	((uint16_t*)configuration_descriptor)[1] = sizeof(configuration_descriptor);
}

int utf8_to_codepoint(const unsigned char *utf8, int *out_len) {
    if (utf8[0] < 0x80) {  // 1-byte ASCII
        *out_len = 1;
        return utf8[0];
    } else if ((utf8[0] & 0xE0) == 0xC0) {  // 2-byte sequence
        *out_len = 2;
        return ((utf8[0] & 0x1F) << 6) |
               (utf8[1] & 0x3F);
    } else if ((utf8[0] & 0xF0) == 0xE0) {  // 3-byte sequence
        *out_len = 3;
        return ((utf8[0] & 0x0F) << 12) |
               ((utf8[1] & 0x3F) << 6) |
               (utf8[2] & 0x3F);
    } else if ((utf8[0] & 0xF8) == 0xF0) {  // 4-byte sequence
        *out_len = 4;
        return ((utf8[0] & 0x07) << 18) |
               ((utf8[1] & 0x3F) << 12) |
               ((utf8[2] & 0x3F) << 6) |
               (utf8[3] & 0x3F);
    } else {
        *out_len = 1;
        return -1; // invalid
    }
}

uint8_t USB_Encode_String(uint8_t *buff, const char *string) {
	int result = 0;
	int in_idx = 0;
	int out_len = 0;
	for (int i = 0; i < strlen(string); i++) {
		if (in_idx >= strlen(string)) break;
		int string_code = utf8_to_codepoint((uint8_t*)(string + in_idx), &out_len);
		if (string_code > 0) {
			memcpy(buff + result, &string_code, out_len);
			result += 2;
			in_idx += out_len;
		} else {
			break;
		}
	}
	return result;

//	uint8_t out_idx = 0;
//	for (int i = 0; i < strlen(string); i++) {
//		uint8_t lo = string[i];
//		uint8_t hi = 0x00;
//		if (!is_ascii) {
//			hi = string[++i];
//		}
//		buff[out_idx++] = lo;
//		buff[out_idx++] = hi;
//	}
//
//	return out_idx;
}


int last_tick = 0;
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
	uint8_t request_type = ((uint8_t*)hpcd->Setup)[0];
	uint8_t request = ((uint8_t*)hpcd->Setup)[1];
	uint8_t data1 = ((uint8_t*)hpcd->Setup)[2];
	uint8_t data2 = ((uint8_t*)hpcd->Setup)[3];
	uint16_t index = ((uint16_t*)hpcd->Setup)[2];
	uint8_t requested_length = ((uint16_t*)hpcd->Setup)[3];

	if (request_type == STANDARD && request == GET_DESCRIPTOR && data2 == DESCRIPTOR_DEVICE) {
		printf("Sending device descriptor\n");
		HAL_PCD_EP_Transmit(hpcd, 0x00, device_descriptor, sizeof(device_descriptor));
	} else if (request_type == STANDARD && request == GET_DESCRIPTOR && data2 == DESCRIPTOR_CONFIGURATION) {
		printf("Sending configuration descriptor\n");

		if (requested_length > sizeof(configuration_descriptor)) requested_length = sizeof(configuration_descriptor);

		HAL_PCD_EP_Transmit(hpcd, 0x00, configuration_descriptor, requested_length);
	} else if (request_type == STANDARD && request == GET_DESCRIPTOR && data2 == DESCRIPTOR_DEVICE_QUALIFIER) {
//		printf("Sending qualifier descriptor\n");
		HAL_PCD_EP_Transmit(hpcd, 0x00, 0, 0);
	} else if (request_type == STANDARD_HTD && request == SET_ADDRESS) {
		printf("Setting address: %i\n", data1);
		HAL_PCD_SetAddress(hpcd, data1);
		HAL_PCD_EP_Transmit(hpcd, 0, 0, 0);
	} else if (request_type == STANDARD_HTD && request == SET_CONFIGURATION) {
		printf("Setting configuration, %i\n", data1);

		// Open  bulk endpoint
		HAL_PCD_EP_Flush(hpcd, 0x81);
		HAL_PCD_EP_Open(hpcd, 0x81, 64, EP_TYPE_BULK);
		HAL_PCD_EP_Transmit(hpcd, 0x81, 0, 0);

		HAL_PCD_EP_Transmit(hpcd, 0, 0, 0);
	} else if (request_type == STANDARD && request == GET_DESCRIPTOR && data2 == DESCRIPTOR_STRING) {
		printf("Sending string %i with index %i of size %i\n", data1, index, requested_length);
		switch(data1) {
		case 0:
			lang_string_descriptor[0] = sizeof(lang_string_descriptor);
			HAL_PCD_EP_Transmit(hpcd, 0x00, lang_string_descriptor, sizeof(lang_string_descriptor));
//			printf("Sending string descriptor of size %i\n", sizeof(lang_string_descriptor));
			break;
		case 1:
			const char* manufacturer_name = "Виробник!!";
			string_descriptor[0] = 2 + USB_Encode_String(string_descriptor + 2, manufacturer_name);
			HAL_PCD_EP_Transmit(hpcd, 0x00, string_descriptor, string_descriptor[0]);
//			printf("Sending string descriptor of size %i\n", string_descriptor[0]);
			break;
		case 2:
			//			const char* product_name = "Пристрій!!";
			const char* product_name = "Hello!";
			string_descriptor[0] = 2 + USB_Encode_String(string_descriptor + 2, product_name);
			HAL_PCD_EP_Transmit(hpcd, 0x00, string_descriptor, string_descriptor[0]);
//			printf("Sending string descriptor of size %i\n", string_descriptor[0]);
			break;
		case 3:
			const char* serial_name = "Серійний номер!";
			string_descriptor[0] = 2 + USB_Encode_String(string_descriptor + 2, serial_name);
			HAL_PCD_EP_Transmit(hpcd, 0x00, string_descriptor, string_descriptor[0]);
//			printf("Sending string descriptor of size %i\n", string_descriptor[0]);
			break;
		case 4:
			const char* interface_name = "Інтерфейс!";
			string_descriptor[0] = 2 + USB_Encode_String(string_descriptor + 2, interface_name);
			HAL_PCD_EP_Transmit(hpcd, 0x00, string_descriptor, string_descriptor[0]);
//			printf("Sending string descriptor of size %i\n", string_descriptor[0]);
			break;

		}
	}


	// Custom requests start here
	if (request_type == MY_CUSTOM_OUT_REQUEST_TYPE && request == MY_CUSTOM_REQUEST) {
		HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
		HAL_PCD_EP_Transmit(hpcd, 0, 0, 0);
	}
	if (request_type == MY_CUSTOM_IN_REQUEST_TYPE && request == MY_CUSTOM_REQUEST) {
		uint8_t response[4];
		uint16_t transfer_len = 0;
		if (is_half1_available) {
			is_half1_available = 0;
			transfer_len = SOUND_ITEMS / 2;
			HAL_PCD_EP_Transmit(hpcd, 0x81, (uint8_t*)sound, transfer_len);
		} else if (is_half2_available) {
			is_half2_available = 0;
			transfer_len = SOUND_ITEMS / 2;
			HAL_PCD_EP_Transmit(hpcd, 0x81, (uint8_t*)(sound) + SOUND_ITEMS / 2, transfer_len);
		}
		*(uint16_t*)(response) = transfer_len;
		*(response + 2) = is_overrun;
		HAL_PCD_EP_Transmit(hpcd, 0, response, 4);
		is_overrun = 0;

//		printf("Arming bulk transfer for %i bytes.\n", transfer_len);
	}
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
	printf("Data OUT stage, ep %i\n", epnum);
	if (epnum == 0) {
		printf("Data OUT stage, ep %i\n", epnum);
		HAL_PCD_EP_Transmit(hpcd, 0, 0, 0);
	}
}
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
	printf("Data IN stage, ep %i\n", epnum);
	if (epnum == 0) 	HAL_PCD_EP_Receive(hpcd, 0x00, 0, 0);
	if (epnum == 1) 	printf("Bulk transfer complete.\n");
}

void USB_Start_M3() {
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS , 0x00 , PCD_SNG_BUF, 0x18);
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS , 0x80 , PCD_SNG_BUF, 0x58);

  // IN Bulk endpoint (1)
  uint32_t bulk_buff_1 = 0x58 + SOUND_ITEMS/4;
  uint32_t bulk_buff_2 = bulk_buff_1 + SOUND_ITEMS/4;
  HAL_PCDEx_PMAConfig(&hpcd_USB_FS , 0x81 , PCD_DBL_BUF, (bulk_buff_2 << 16) + bulk_buff_1);

  HAL_PCD_Start(&hpcd_USB_FS);
}
