#include <stdio.h>
#include <string.h>
#include "libredxx/libredxx.h"
#include "ft260.h"

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

	struct libredxx_ft260_feature_out_report rep_ftr_out;
	struct libredxx_ft260_output_report rep_out;
	struct libredxx_ft260_input_report rep_in;
	size_t size = 0;

	// set GPIO G function
	memset(&rep_ftr_out, 0, sizeof(rep_ftr_out));
	size = sizeof(rep_ftr_out);
	rep_ftr_out.report_id = 0xA1;
	rep_ftr_out.gpio_function.request = 0x09;
	rep_ftr_out.gpio_function.function = 0;
	status = libredxx_write(device, rep_ftr_out.bytes, &size, LIBREDXX_ENDPOINT_FEATURE);

	// set GPIO G direction
	memset(&rep_ftr_out, 0, sizeof(rep_ftr_out));
	size = sizeof(rep_ftr_out);
	rep_ftr_out.report_id = 0xB0;
	rep_ftr_out.gpio_write.gpio_dir_ex = 1 << 6;
	status = libredxx_write(device, rep_ftr_out.bytes, &size, LIBREDXX_ENDPOINT_FEATURE);

	// set GPIO value
	memset(&rep_ftr_out, 0, sizeof(rep_ftr_out));
	size = sizeof(rep_ftr_out);
	rep_ftr_out.report_id = 0xB0;
	rep_ftr_out.gpio_write.gpio_dir_ex = 1 << 6;
	rep_ftr_out.gpio_write.gpio_val_ex = 1 << 6;
	status = libredxx_write(device, rep_ftr_out.bytes, &size, LIBREDXX_ENDPOINT_FEATURE);

	// set I2C clock speed to 100 Kbps
	memset(&rep_ftr_out, 0, sizeof(rep_ftr_out));
	size = sizeof(rep_ftr_out);
	rep_ftr_out.report_id = 0xA1;
	rep_ftr_out.i2c_clock_speed.request = 0x22;
	rep_ftr_out.i2c_clock_speed.speed_lsb = 0x64; // 100kbps
	rep_ftr_out.i2c_clock_speed.speed_msb = 0;
	status = libredxx_write(device, rep_ftr_out.bytes, &size, LIBREDXX_ENDPOINT_FEATURE);

	// write I2C control byte for MSA
	memset(&rep_out, 0, sizeof(rep_out));
	size = sizeof(rep_out);
	rep_out.report_id = 0xD0;
	rep_out.i2c_write_request.slave_addr = 0x50;
	rep_out.i2c_write_request.flags = 0x06; // START | STOP
	rep_out.i2c_write_request.length = 1;
	rep_out.i2c_write_request.data[0] = 0x00;
	status = libredxx_write(device, rep_out.bytes, &size, LIBREDXX_ENDPOINT_IO);

	// request I2C read for SFP MSA
	memset(&rep_out, 0, sizeof(rep_out));
	size = sizeof(rep_out);
	rep_out.report_id = 0xC2; // I2C Read Request
	rep_out.i2c_read_request.slave_addr = 0x50;
	rep_out.i2c_read_request.flags = 0x06; // START | STOP
	rep_out.i2c_read_request.length = 256;
	status = libredxx_write(device, rep_out.bytes, &size, LIBREDXX_ENDPOINT_IO);

	// Read Loop
	uint8_t msa_table[256];
	size_t bytes_read = 0;

	while (bytes_read < 256) {
		size = sizeof(rep_in);
		status = libredxx_read(device, rep_in.bytes, &size, LIBREDXX_ENDPOINT_IO);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			return -1;
		}

		// check report ID for valid I2C input data (0xD0 - 0xDE)
		if (rep_in.report_id >= 0xD0 && rep_in.report_id <= 0xDE) {
			size_t chunk_len = rep_in.i2c_read.length;
			if (bytes_read + chunk_len > 256) {
				chunk_len = 256 - bytes_read;
			}
			memcpy(msa_table + bytes_read, rep_in.i2c_read.data, chunk_len);
			bytes_read += chunk_len;
		}
	}

	if (bytes_read == 256) {
		print_msa_details(msa_table);
	}

	return 0;
}