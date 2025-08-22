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

#ifndef LIBREDXX_LIBREDXX_H
#define LIBREDXX_LIBREDXX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum libredxx_device_type {
	LIBREDXX_DEVICE_TYPE_D2XX,
	LIBREDXX_DEVICE_TYPE_D3XX,
};
typedef enum libredxx_device_type libredxx_device_type;

struct libredxx_serial {
	char serial[16];
};
typedef struct libredxx_serial libredxx_serial;

struct libredxx_device_id {
	uint16_t vid;
	uint16_t pid;
};
typedef struct libredxx_device_id libredxx_device_id;

struct libredxx_find_filter {
	libredxx_device_type type;
	libredxx_device_id id;
};
typedef struct libredxx_find_filter libredxx_find_filter;

enum libredxx_status {
	LIBREDXX_STATUS_SUCCESS,
	LIBREDXX_STATUS_ERROR_SYS, // system error, for details call GetLastError(), etc
	LIBREDXX_STATUS_ERROR_INTERRUPTED,
	LIBREDXX_STATUS_ERROR_OVERFLOW,
	LIBREDXX_STATUS_ERROR_IO, // invalid IO with the device
	LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT,
};
typedef enum libredxx_status libredxx_status;

typedef struct libredxx_found_device libredxx_found_device;

typedef struct libredxx_opened_device libredxx_opened_device;

libredxx_status libredxx_find_devices(const libredxx_find_filter* filters, size_t filters_count, libredxx_found_device*** devices, size_t* devices_count);
libredxx_status libredxx_free_found(libredxx_found_device** devices);

libredxx_status libredxx_get_serial(const libredxx_found_device* found, libredxx_serial* serial);

libredxx_status libredxx_get_device_id(const libredxx_found_device* found, libredxx_device_id* id);
libredxx_status libredxx_get_device_type(const libredxx_found_device* found, libredxx_device_type* type);

libredxx_status libredxx_open_device(const libredxx_found_device* found, libredxx_opened_device** opened);
libredxx_status libredxx_close_device(libredxx_opened_device* device);

libredxx_status libredxx_interrupt(libredxx_opened_device* device);

libredxx_status libredxx_read(libredxx_opened_device* device, void* buffer, size_t* buffer_size);
libredxx_status libredxx_write(libredxx_opened_device* device, void* buffer, size_t* buffer_size);

#ifdef __cplusplus
}
#endif

#endif // LIBREDXX_LIBREDXX_H
