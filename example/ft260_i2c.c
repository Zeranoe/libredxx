/*
 * Copyright (c) 2025 Kyle Schwarz <zeranoe@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libredxx/libredxx.h"

#define I2C_MAX_ADDR ((1 << 7) - 1)

#define MAX_WRITE_SIZE 256
#define WRITE_DATA_ARG_START 5


int main(int argc, char** argv) {
	if (argc < 6 || argc > (5 + MAX_WRITE_SIZE)) {
		printf("usage: %s <vid> <pid> <addr> [<read> <size> | <write> <byte_1> <byte_2> ... <byte_%d>]\n", argv[0], MAX_WRITE_SIZE);
		printf("example: %s 0403 0603 20 read 12\n", argv[0]);
		printf("example: %s 0403 0603 30 write 01 02 03\n", argv[0]);
		return -1;
	}

	uint16_t vid_arg = (uint16_t)strtoul(argv[1], NULL, 16);
	uint16_t pid_arg = (uint16_t)strtoul(argv[2], NULL, 16);
	libredxx_found_device** found_devices = NULL;
	size_t found_devices_count = 0;
	libredxx_find_filter filters[] = {
		{
			LIBREDXX_DEVICE_TYPE_FT260,
			{vid_arg, pid_arg}
		}
	};
	size_t filters_count = 1;
	libredxx_status status = libredxx_find_devices(filters, filters_count, &found_devices, &found_devices_count);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		printf("error: failed to find devices: %d\n", status);
		return -1; // no need to free devices on failure
	}
	if (found_devices_count == 0) {
		printf("warning: no devices found\n");
		return -1;
	}

	uint16_t addr_arg = (uint16_t)strtoul(argv[3], NULL, 16);
	if (addr_arg > I2C_MAX_ADDR) {
		printf("error: invalid I2C address (must be 7-bit)\n");
		return -1;
	}

	if (strcmp(argv[4], "read") == 0) {
		for (size_t i = 0; i < found_devices_count; ++i) {
			// TODO
		}
	} else if (strcmp(argv[4], "write") == 0) {
		size_t write_size = argc - WRITE_DATA_ARG_START;
		if (write_size == 0) {
			printf("warning: no write data specified\n");
			return -1;
		}
		uint8_t write_data[MAX_WRITE_SIZE];
		for (int i = 0; i < write_size; ++i) {
			uint16_t byte = (uint16_t)strtoul(argv[WRITE_DATA_ARG_START + i], NULL, 16);
			if (byte > UINT8_MAX) {
				printf("error: invalid data byte\n");
				return -1;
			}
			write_data[i] = (uint8_t)byte;
		}
		for (size_t i = 0; i < found_devices_count; ++i) {
			// TODO
		}
	} else {
		printf("error: must specify read or write\n");
		return -1;
	}
}