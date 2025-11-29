#include <stdio.h>
#include <time.h>
#include <string.h>
#include <windows.h>
#include "libredxx/libredxx.h"
#include "ft260.h"

#define MSA_SIZE 128

static uint8_t buf[LIBREDXX_FT260_REPORT_SIZE];

void sleep_ms(uint64_t ms)
{
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	nanosleep(&ts, NULL);
#endif
}

void print_msa_details(const uint8_t* data) {
	printf("\n=== SFP MSA Details ===\n");

	// Identifier (Byte 0)
	printf("Identifier:      0x%02X ", data[0]);
	switch (data[0]) {
		case 0x03: printf("(SFP/SFP+/SFP28)\n"); break;
		default:   printf("(Unknown)\n"); break;
	}

	// Connector (Byte 2)
	printf("Connector:       0x%02X ", data[2]);
	switch (data[2]) {
		case 0x01: printf("(SC)\n"); break;
		case 0x07: printf("(LC)\n"); break;
		case 0x21: printf("(Copper Pigtail)\n"); break;
		case 0x22: printf("(RJ45)\n"); break;
		default:   printf("(Other)\n"); break;
	}

	// Transceiver Code (Bytes 3-10)
	printf("Transceiver:     0x%02X... (Raw)\n", data[3]);

	// Encoding (Byte 11)
	printf("Encoding:        0x%02X\n", data[11]);

	// Nominal Bit Rate (Byte 12) - in units of 100MBd
	if (data[12] > 0) {
		printf("Bit Rate:        %d MBd\n", data[12] * 100);
	} else {
		printf("Bit Rate:        Unspecified\n");
	}

	// Lengths (Bytes 14-19)
	printf("Link Length (SM): %d km\n", data[14]); // units of km
	printf("Link Length (OM3): %d m\n", data[15] * 2); // units of 2m
	printf("Link Length (OM2): %d m\n", data[16]); // units of 1m
	printf("Link Length (OM1): %d m\n", data[17]); // units of 1m
	printf("Link Length (Cu):  %d m\n", data[18]); // units of 1m

	// Vendor Name (Bytes 20-35) - ASCII
	char vendor_name[17];
	memcpy(vendor_name, &data[20], 16);
	vendor_name[16] = '\0';
	printf("Vendor Name:     %s\n", vendor_name);

	// Vendor OUI (Bytes 37-39)
	printf("Vendor OUI:      %02X:%02X:%02X\n", data[37], data[38], data[39]);

	// Vendor PN (Bytes 40-55) - ASCII
	char vendor_pn[17];
	memcpy(vendor_pn, &data[40], 16);
	vendor_pn[16] = '\0';
	printf("Vendor PN:       %s\n", vendor_pn);

	// Vendor Rev (Bytes 56-59) - ASCII
	char vendor_rev[5];
	memcpy(vendor_rev, &data[56], 4);
	vendor_rev[4] = '\0';
	printf("Vendor Rev:      %s\n", vendor_rev);

	// Wavelength (Bytes 60-61)
	uint16_t wavelength = (data[60] << 8) | data[61];
	printf("Wavelength:      %d nm\n", wavelength);

	// Vendor SN (Bytes 68-83) - ASCII
	char vendor_sn[17];
	memcpy(vendor_sn, &data[68], 16);
	vendor_sn[16] = '\0';
	printf("Vendor SN:       %s\n", vendor_sn);

	// Date Code (Bytes 84-91) - ASCII (YYMMDDxx)
	char date_code[9];
	memcpy(date_code, &data[84], 8);
	date_code[8] = '\0';
	printf("Date Code:       %s\n", date_code);

	printf("=======================\n");
}

int main() {
	libredxx_find_filter filters[] = {
		{
			LIBREDXX_DEVICE_TYPE_FT260,
			{0x0403, 0x6030}
		}
	};
	size_t filters_count = 1;
	libredxx_found_device** found_devices = NULL;
	size_t found_devices_count = 0;
	libredxx_status status = libredxx_find_devices(filters, filters_count, &found_devices, &found_devices_count);
	if (found_devices_count == 0) {
		return -1;
	}
	libredxx_opened_device* device = NULL;
	status = libredxx_open_device(found_devices[0], &device);

	size_t size = 0;

	// set GPIO G function
	struct libredxx_ft260_feature_out_gpio_function rep_set_gpio_fn = {0};
	size = sizeof(rep_set_gpio_fn);
	rep_set_gpio_fn.report_id = 0xA1;
	rep_set_gpio_fn.request = 0x09;
	rep_set_gpio_fn.function = 0;
	status = libredxx_write(device, &rep_set_gpio_fn, &size, LIBREDXX_ENDPOINT_FEATURE);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}

	// set GPIO G direction
	struct libredxx_ft260_feature_out_gpio rep_set_gpio = {0};
	size = sizeof(rep_set_gpio);
	rep_set_gpio.report_id = 0xB0;
	rep_set_gpio.gpio_dir_ex = 1 << 6;
	status = libredxx_write(device, &rep_set_gpio, &size, LIBREDXX_ENDPOINT_FEATURE);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}

	// set GPIO value
	rep_set_gpio.gpio_val_ex = 1 << 6;
	status = libredxx_write(device, &rep_set_gpio, &size, LIBREDXX_ENDPOINT_FEATURE);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}

	// give SFP time to boot
	sleep_ms(1000);

	// reset I2C
	struct libredxx_ft260_feature_out_i2c_reset rep_i2c_reset = {0};
	size = sizeof(rep_i2c_reset);
	rep_i2c_reset.report_id = 0xA1;
	rep_i2c_reset.request = 0x20;
	status = libredxx_write(device, &rep_i2c_reset, &size, LIBREDXX_ENDPOINT_FEATURE);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}

	// set I2C clock speed to 100 Kbps
	struct libredxx_ft260_feature_out_i2c_speed rep_set_i2c_speed = {0};
	size = sizeof(rep_set_i2c_speed);
	rep_set_i2c_speed.report_id = 0xA1;
	rep_set_i2c_speed.request = 0x22;
	rep_set_i2c_speed.speed_lsb = 0x64; // 100kbps
	rep_set_i2c_speed.speed_msb = 0;
	status = libredxx_write(device, &rep_set_i2c_speed, &size, LIBREDXX_ENDPOINT_FEATURE);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}

	// check if SFP connected
	struct libredxx_ft260_feature_in_gpio rep_get_gpio = {0};
	size = sizeof(rep_get_gpio);
	rep_get_gpio.report_id = 0xB0;
	status = libredxx_read(device, &rep_get_gpio, &size, LIBREDXX_ENDPOINT_FEATURE);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}
	bool is_sfp_connected = (rep_get_gpio.gpio_val_ex & (1 << 3)) ? 0 : 1;
	if (!is_sfp_connected) {
		printf("No SFP connected to EZ-SFP\n");
		return 0;
	}

	// write I2C control byte for MSA
	struct libredxx_ft260_out_i2c_write* rep_i2c_write = (struct libredxx_ft260_out_i2c_write*)buf;
	size = 64;
	memset(rep_i2c_write, 0, size);
	rep_i2c_write->report_id = 0xDE;
	rep_i2c_write->slave_addr = 0x50;
	rep_i2c_write->flags = 0x06; // START | STOP
	rep_i2c_write->length = 1;
	rep_i2c_write->data[0] = 0x00;
	status = libredxx_write(device, rep_i2c_write, &size, LIBREDXX_ENDPOINT_IO);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}

	// request I2C read for SFP MSA
	struct libredxx_ft260_out_i2c_read* rep_i2c_read_out = (struct libredxx_ft260_out_i2c_read*)buf;
	size = sizeof(struct libredxx_ft260_out_i2c_read);
	memset(rep_i2c_write, 0, size);
	rep_i2c_read_out->report_id = 0xC2; // I2C Read Request
	rep_i2c_read_out->slave_addr = 0x50;
	rep_i2c_read_out->flags = 0x06; // START | STOP
	rep_i2c_read_out->length = MSA_SIZE;
	status = libredxx_write(device, rep_i2c_read_out, &size, LIBREDXX_ENDPOINT_IO);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return -1;
	}

	// Read Loop
	uint8_t msa_table[MSA_SIZE];
	size_t bytes_read = 0;
	while (bytes_read < sizeof(msa_table)) {
		struct libredxx_ft260_in_i2c_read* rep_i2c_read_in = (struct libredxx_ft260_in_i2c_read*)buf;
		size = LIBREDXX_FT260_REPORT_SIZE;
		status = libredxx_read(device, rep_i2c_read_in, &size, LIBREDXX_ENDPOINT_IO);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			return -1;
		}

		// check report ID for valid I2C input data (0xD0 - 0xDE)
		if (rep_i2c_read_in->report_id >= 0xD0 && rep_i2c_read_in->report_id <= 0xDE) {
			size_t chunk_len = rep_i2c_read_in->length;
			if (bytes_read + chunk_len > sizeof(msa_table)) {
				chunk_len = sizeof(msa_table) - bytes_read;
			}
			memcpy(msa_table + bytes_read, rep_i2c_read_in->data, chunk_len);
			bytes_read += chunk_len;
		}
	}

	if (bytes_read == sizeof(msa_table)) {
		print_msa_details(msa_table);
	}

	libredxx_free_found(found_devices);
	libredxx_close_device(device);

	return 0;
}