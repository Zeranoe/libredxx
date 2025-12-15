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

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libredxx/libredxx.h"

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
	arg->read_status = libredxx_read(arg->device, arg->rx, &arg->rx_size, LIBREDXX_ENDPOINT_A);
	#ifdef _WIN32
	return 0;
	#else
	return NULL;
	#endif
}

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

static void opened_device_scope(libredxx_opened_device* opened, char* serial_arg, uint8_t* tx, size_t tx_len)
{
	libredxx_status status;
	printf("info: opened target device with serial '%s'\n", serial_arg);
	// spawn the read thread before writing anything
	uint8_t rx[64];
	struct read_thread_arg arg = {
		.device = opened,
		.rx = rx,
		.rx_size = sizeof(rx)
	};
	#ifdef _WIN32
	HANDLE thread = CreateThread(NULL, 0, read_thread, &arg, 0, NULL);
	#else
	pthread_t thread;
	pthread_create(&thread, NULL, read_thread, &arg);
	#endif
	sleep_ms(100); // let thread start
	status = libredxx_write(opened, tx, &tx_len, LIBREDXX_ENDPOINT_A);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		printf("error: unable to write to device: %d\n", status);
	}

	sleep_ms(100); // wait 100ms for response to arrive

	libredxx_interrupt(opened); // timeout

	#ifdef _WIN32
	WaitForMultipleObjects(1, &thread, TRUE, INFINITE);
	CloseHandle(thread);
	#else
	pthread_join(thread, NULL);
	#endif

	if (arg.read_status != LIBREDXX_STATUS_SUCCESS) {
		printf("error: read failed: %d\n", arg.read_status);
		return;
	}

	printf("info: read %zd bytes:", arg.rx_size);
	for (size_t i = 0; i < arg.rx_size; ++i) {
		printf(" %02X", arg.rx[i]);
	}
	printf("\n");

}

int main(int argc, char** argv)
{
	if (argc != 6) {
		printf("usage: %s <vid> <pid> <serial> <d2xx | d3xx> <tx>\n", argv[0]);
		printf("example: %s 0403 601F FT601 d3xx AABBCCDDEEFF\n", argv[0]);
		return -1;
	}
	uint16_t vid_arg = (uint16_t)strtoul(argv[1], NULL, 16);
	uint16_t pid_arg = (uint16_t)strtoul(argv[2], NULL, 16);
	char* serial_arg = argv[3];

	// convert the device type
	char* type_str = argv[4];
	libredxx_device_type type;
	if (strcmp(type_str, "d2xx") == 0) {
		type = LIBREDXX_DEVICE_TYPE_D2XX;
	} else if (strcmp(type_str, "d3xx") == 0) {
		type = LIBREDXX_DEVICE_TYPE_D3XX;
	} else {
		printf("error: invalid device type, must be \"d2xx\" or \"d3xx\"\n");
		return -1;
	}

	// get the tx data in bytes
	char* tx_str = argv[5];
	size_t tx_str_len = strlen(tx_str);
	uint8_t tx[512];
	size_t tx_len = tx_str_len / 2;
	for (size_t i = 0; i < tx_len; ++i) {
		sscanf(tx_str + (i * 2), "%2hhx", &tx[i]);
	}

	libredxx_status status;

	libredxx_find_filter filters[] = {
		{
			type,
			{ vid_arg, pid_arg }
		}
	};
	size_t filters_count = 1;

	libredxx_found_device** found_devices = NULL;
	size_t found_devices_count = 0;
	status = libredxx_find_devices(filters, filters_count, &found_devices, &found_devices_count);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		printf("error: failed to find devices: %d\n", status);
		return -1; // no need to free devices on failure
	}
	if (found_devices_count == 0) {
		printf("warning: no devices found\n");
		return -1;
	}
	for (size_t i = 0; i < found_devices_count; ++i) {
		libredxx_serial device_serial;
		status = libredxx_get_serial(found_devices[i], &device_serial);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			printf("error: unable to get serial: %d\n", status);
			continue;
		}
		if (strcmp(device_serial.serial, serial_arg) != 0) {
			continue; // not the desired serial
		}
		libredxx_opened_device* opened = NULL;
		status = libredxx_open_device(found_devices[i], &opened);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			printf("error: unable to open device: %d\n", status);
			continue; // no need to free device on failure
		}
		opened_device_scope(opened, serial_arg, tx, tx_len);
		libredxx_close_device(opened);
		opened = NULL;
	}

	libredxx_free_found(found_devices);

	return 0;
}
