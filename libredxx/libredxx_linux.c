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

#include "libredxx.h"

#include <dirent.h>
#include <sys/types.h>
#include <linux/usbdevice_fs.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <linux/usb/ch9.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <poll.h>

#define USBFS_PATH "/dev/bus/usb"
#define D2XX_HEADER_SIZE 2

struct libredxx_found_device {
	char path[512];
	libredxx_device_id id;
	libredxx_device_type type;
};

struct libredxx_opened_device {
	libredxx_found_device found;
	int handle;
	int d3xx_pipes[2];
	uint8_t* d2xx_rx_buffer;
	size_t d2xx_rx_buffer_size;
	bool read_interrupted;
};

#pragma pack(push, 1)
struct usb_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
};
#pragma pack(pop)

libredxx_status libredxx_find_devices(const libredxx_find_filter* filters, size_t filters_count, libredxx_found_device*** devices, size_t* devices_count)
{
	libredxx_status status = LIBREDXX_STATUS_SUCCESS;
	DIR* usb = opendir(USBFS_PATH);
	struct dirent* bus_entry;
	size_t device_index = 0;
	libredxx_found_device* private_devices = NULL;
	while (status == LIBREDXX_STATUS_SUCCESS && (bus_entry = readdir(usb))) {
		if (bus_entry->d_name[0] == '.') {
			continue;
		}
		char bus_path[32];
		if (snprintf(bus_path, sizeof(bus_path), USBFS_PATH "/%s", bus_entry->d_name) == -1) {
			status = LIBREDXX_STATUS_ERROR_SYS;
			break;
		}
		DIR* bus = opendir(bus_path);
		struct dirent* device_entry;
		while ((device_entry = readdir(bus))) {
			if (device_entry->d_name[0] == '.') {
				continue;
			}
			char device_path[32];
			if (snprintf(device_path, sizeof(device_path), USBFS_PATH "/%s/%s", bus_entry->d_name, device_entry->d_name) == -1) {
				status = LIBREDXX_STATUS_ERROR_SYS;
				break;
			}
			struct usb_descriptor descriptor = {0};
			int fd = open(device_path, O_RDONLY);
			read(fd, &descriptor, sizeof(descriptor));
			close(fd);
			for (size_t i = 0; i < filters_count; ++i) {
				const libredxx_find_filter* filter = &filters[i];
				if (descriptor.idVendor == filter->id.vid && descriptor.idProduct == filter->id.pid) {
					private_devices = realloc(private_devices, sizeof(libredxx_found_device) * (device_index + 1));
					libredxx_found_device* private_device = &private_devices[device_index];
					private_device->id.vid = descriptor.idVendor;
					private_device->id.pid = descriptor.idProduct;
					strcpy(private_device->path, device_path);
					private_device->type = filter->type;
					++device_index;
				}
			}
		}
		closedir(bus);
	}
	closedir(usb);
	*devices_count = device_index;
	*devices = NULL;
	if (*devices_count > 0) {
		*devices = malloc(sizeof(libredxx_found_device*) * *devices_count);
		for (size_t i = 0; i < *devices_count; ++i) {
			(*devices)[i] = &private_devices[i];
		}
	}
	return status;
}

libredxx_status libredxx_free_found(libredxx_found_device** devices)
{
	free(devices[0]);
	free(devices);
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_get_device_id(const libredxx_found_device* found, libredxx_device_id* id)
{
	*id = found->id;
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_get_device_type(const libredxx_found_device* found, libredxx_device_type* type)
{
	*type = found->type;
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_open_device(const libredxx_found_device* found, libredxx_opened_device** opened)
{
	int handle = open(found->path, O_RDWR);
	if (handle == -1) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	libredxx_opened_device* private_opened = calloc(1, sizeof(libredxx_opened_device));
	if (!private_opened) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	private_opened->found = *found;
	private_opened->handle = handle;
	if (found->type == LIBREDXX_DEVICE_TYPE_D3XX) {
		pipe(private_opened->d3xx_pipes);
	}
	*opened = private_opened;
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_close_device(libredxx_opened_device* device)
{
	libredxx_interrupt(device);
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D3XX) {
		close(device->d3xx_pipes[1]);
		close(device->d3xx_pipes[0]);
	} else {
		free(device->d2xx_rx_buffer);
	}
	close(device->handle);
	free(device);
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_get_serial(const libredxx_opened_device* device, libredxx_serial* serial)
{
	uint8_t raw[255] = {0};
	struct usbdevfs_ctrltransfer control = {0};
	control.bRequestType = USB_DIR_IN;
	control.bRequest = USB_REQ_GET_DESCRIPTOR;
	control.wValue = (USB_DT_STRING << 8) | 3; // serial
	control.wIndex = 0x0409; // English
	control.wLength = sizeof(raw);
	control.data = raw;
	int r = ioctl(device->handle, USBDEVFS_CONTROL, &control);
	if (r == -1) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	if (r < 2 /* size (u8) + type (u8) */) {
		return LIBREDXX_STATUS_ERROR_IO;
	}
	if (raw[1] != USB_DT_STRING) {
		return LIBREDXX_STATUS_ERROR_IO;
	}
	// TODO: make sure we have enough room in serial to write this
	char* out = serial->serial;
	for (int i = 2; i < r; i += 2) {
		*out++ = raw[i];
	}
	*out = '\0';
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_interrupt(libredxx_opened_device* device)
{
	device->read_interrupted = true;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D3XX) {
		uint64_t one = 1;
		write(device->d3xx_pipes[1], &one, sizeof(one));
	}
	return LIBREDXX_STATUS_SUCCESS;
}

static libredxx_status libredxx_d3xx_trigger_read(libredxx_opened_device* device, uint32_t size)
{
	uint8_t* size_bytes = (uint8_t*)&size;
	uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x82, 0x01, 0x00, 0x00, size_bytes[0], size_bytes[1], size_bytes[2], size_bytes[3], 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	struct usbdevfs_bulktransfer bulk = {0};
	bulk.ep = 0x01;
	bulk.len = sizeof(data);
	bulk.data = data;
	return ioctl(device->handle, USBDEVFS_BULK, &bulk) == -1 ? LIBREDXX_STATUS_ERROR_SYS : LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_read(libredxx_opened_device* device, void* buffer, size_t* buffer_size)
{
	libredxx_status status;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D3XX) {
		status = libredxx_d3xx_trigger_read(device, *buffer_size);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			return status;
		}

		struct usbdevfs_urb urb = {0};
		urb.type = USBDEVFS_URB_TYPE_BULK;
		urb.endpoint = 0x82;
		urb.buffer = buffer;
		urb.buffer_length = *buffer_size;

		if (ioctl(device->handle, USBDEVFS_SUBMITURB, &urb) != 0) {
			return LIBREDXX_STATUS_ERROR_SYS;
		}
		struct pollfd fds[2] = {0};
		fds[0].fd = device->handle;
		fds[0].events = POLLOUT;
		// for int
		fds[1].fd = device->d3xx_pipes[0];
		fds[1].events = POLLIN;
		if (poll(fds, 2, -1) < 0) {
			return LIBREDXX_STATUS_ERROR_SYS;
		}

		if (device->read_interrupted) {
			return LIBREDXX_STATUS_ERROR_INTERRUPTED;
		}

		if (ioctl(device->handle, USBDEVFS_REAPURB, &urb) != 0) {
			return LIBREDXX_STATUS_ERROR_SYS;
		}

		*buffer_size = urb.actual_length;
		return LIBREDXX_STATUS_SUCCESS;
	} else {
		size_t headered_buffer_size = *buffer_size + D2XX_HEADER_SIZE;
		if (headered_buffer_size > device->d2xx_rx_buffer_size) {
			device->d2xx_rx_buffer = realloc(device->d2xx_rx_buffer, headered_buffer_size);
			device->d2xx_rx_buffer_size = headered_buffer_size;
		}
		struct usbdevfs_bulktransfer bulk = {0};
		bulk.ep = 0x81;
		bulk.len = headered_buffer_size;
		bulk.data = device->d2xx_rx_buffer;
		device->read_interrupted = false;
		while (true) {
			int r = ioctl(device->handle, USBDEVFS_BULK, &bulk);
			if (r == -1) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			if (r > 2) {
				*buffer_size = r - 2;
				memcpy(buffer, &device->d2xx_rx_buffer[2], *buffer_size);
				return LIBREDXX_STATUS_SUCCESS;
			}
			if (device->read_interrupted) {
				return LIBREDXX_STATUS_ERROR_INTERRUPTED;
			}
		}

	}
}

libredxx_status libredxx_write(libredxx_opened_device* device, void* buffer, size_t* buffer_size)
{
	struct usbdevfs_bulktransfer bulk = {0};
	bulk.ep = 0x02;
	bulk.len = *buffer_size;
	bulk.data = buffer;
	int r = ioctl(device->handle, USBDEVFS_BULK, &bulk);
	return r == -1 ? LIBREDXX_STATUS_ERROR_SYS : LIBREDXX_STATUS_SUCCESS;
}
