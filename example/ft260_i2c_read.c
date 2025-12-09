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
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif
#include "libredxx/libredxx.h"
#include "ft260.h"

#define I2C_MAX_ADDR ((1 << 7) - 1)

#define ARG_VID_POS 1
#define ARG_PID_POS 2
#define ARG_ADDR_POS 3
#define ARG_WRITE_CTRL_POS 4
#define ARG_SIZE_POS 5

struct read_thread_arg {
    libredxx_opened_device* device;
    uint8_t* rx;
    size_t rx_size;
    libredxx_status read_status;
};

#ifdef _WIN32
static DWORD read_thread(void* varg)
#else
static void* read_thread(void* varg)
#endif
{
    struct read_thread_arg* arg = (struct read_thread_arg*)varg;
    arg->read_status = libredxx_read(arg->device, arg->rx, &arg->rx_size, LIBREDXX_ENDPOINT_IO);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void sleep_ms(unsigned int ms)
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

int main(int argc, char** argv) {
	if (argc != 6) {
		printf("usage: %s <vid> <pid> <addr> <write_ctrl> <size>\n", argv[0]);
		printf("example: %s 0403 6030 50 1 80\n", argv[0]);
		return -1;
	}

	uint16_t vid = (uint16_t)strtoul(argv[ARG_VID_POS], NULL, 16);
	uint16_t pid = (uint16_t)strtoul(argv[ARG_PID_POS], NULL, 16);
	libredxx_found_device** found_devices = NULL;
	size_t found_devices_count = 0;
	libredxx_find_filter filters[] = {
		{
			LIBREDXX_DEVICE_TYPE_FT260,
			{vid, pid}
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

	uint16_t addr = (uint16_t)strtoul(argv[ARG_ADDR_POS], NULL, 16);
	if (addr > I2C_MAX_ADDR) {
		printf("error: invalid I2C address (must be 7-bit)\n");
		return -1;
	}

	const size_t read_size = strtoul(argv[ARG_SIZE_POS], NULL, 16);
	if (read_size > UINT8_MAX) {
		printf("error: read size too large\n");
		return -1;
	}

	const int write_ctrl = !!strtol(argv[ARG_WRITE_CTRL_POS], NULL, 16);

	libredxx_opened_device* device = NULL;
	size_t size = 0;

	for (size_t i = 0; i < found_devices_count; ++i) {
		status = libredxx_open_device(found_devices[i], &device);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			printf("error: unable to open device: %d\n", status);
			libredxx_free_found(found_devices);
			return -1;
		}
		if (write_ctrl) {
			struct libredxx_ft260_out_i2c_write rep_i2c_write = {0};
			size = sizeof(rep_i2c_write);
			rep_i2c_write.report_id = 0xDE;
			rep_i2c_write.slave_addr = addr;
			rep_i2c_write.flags = 0x06; // START | STOP
			rep_i2c_write.length = 1;
			rep_i2c_write.data[0] = 0x00;
			status = libredxx_write(device, &rep_i2c_write, &size, LIBREDXX_ENDPOINT_IO);
			if (status != LIBREDXX_STATUS_SUCCESS) {
				printf("error: failed to write control byte\n");
				goto ERROR_EXIT;
			}
		}

		// request read
		struct libredxx_ft260_out_i2c_read rep_i2c_read_out = {0};
		size = sizeof(rep_i2c_read_out);
		rep_i2c_read_out.report_id = 0xC2; // I2C Read Request
		rep_i2c_read_out.slave_addr = addr;
		rep_i2c_read_out.flags = 0x06; // START | STOP
		rep_i2c_read_out.length = read_size;
		status = libredxx_write(device, &rep_i2c_read_out, &size, LIBREDXX_ENDPOINT_IO);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			printf("error: failed read request\n");
			goto ERROR_EXIT;
		}

		// read loop
		printf("======== Found device %zu i2c data ========\n", i);
		size_t rem = read_size;
		while (rem) {
			struct libredxx_ft260_in_i2c_read rep_i2c_read_in = {0};
			struct read_thread_arg arg = {
				.device = device,
				.rx = (uint8_t*)&rep_i2c_read_in,
				.rx_size = sizeof(rep_i2c_read_in)
			};
#ifdef _WIN32
			HANDLE thread = CreateThread(NULL, 0, read_thread, &arg, 0, NULL);
#else
			pthread_t thread;
			pthread_create(&thread, NULL, read_thread, &arg);
#endif
			sleep_ms(200); // let thread read
			libredxx_interrupt(device);

#ifdef _WIN32
			WaitForMultipleObjects(1, &thread, TRUE, INFINITE);
			CloseHandle(thread);
#else
			pthread_join(thread, NULL);
#endif

			if (arg.read_status == LIBREDXX_STATUS_ERROR_INTERRUPTED) {
				printf("error: read timed out\n");
				return -1;
			}
			if (arg.read_status != LIBREDXX_STATUS_SUCCESS) {
				printf("error: read failed\n");
				return -1;
			}

			for (int j = 0; j < rep_i2c_read_in.length; ++j) {
				printf("0x%X\n", rep_i2c_read_in.data[j]);
			}
			rem -= rep_i2c_read_in.length;
		}
		printf("\n");
		libredxx_close_device(device);
	}
	libredxx_free_found(found_devices);
	return 0;

ERROR_EXIT:
	libredxx_close_device(device);
	libredxx_free_found(found_devices);
	return -1;
}