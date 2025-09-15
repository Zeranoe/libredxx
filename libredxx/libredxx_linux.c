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
#define SYSFS_DEVICES_PATH "/sys/bus/usb/devices"
#define D2XX_HEADER_SIZE 2

struct libredxx_found_device {
	char path[512];
	libredxx_serial serial;
	libredxx_device_id id;
	libredxx_device_type type;
	uint8_t interface_count;
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

static ssize_t libredxx_read_text_file(const char* path, void* buffer, size_t buffer_size)
{
	const int fd = open(path, O_RDONLY);
	if (fd == -1) {
		return -1;
	}
	ssize_t size = read(fd, buffer, buffer_size);
	close(fd);
	if (size > 0) {
		((uint8_t*)buffer)[size - 1] = '\0'; // newline terminated to null terminated
	}
	return size;
}

static const libredxx_find_filter* libredxx_match_filter(uint16_t vid, uint16_t pid, const libredxx_find_filter* filters, size_t filters_count)
{
	for (size_t i = 0; i < filters_count; ++i) {
		const libredxx_find_filter* filter = &filters[i];
		if (vid == filter->id.vid && pid == filter->id.pid) {
			return filter;
		}
	}
	return NULL;
}

libredxx_status libredxx_find_devices(const libredxx_find_filter* filters, size_t filters_count, libredxx_found_device*** devices, size_t* devices_count)
{
	libredxx_status status = LIBREDXX_STATUS_SUCCESS;
	size_t device_index = 0;
	libredxx_found_device* private_devices = NULL;
	DIR* devices_dir = opendir(SYSFS_DEVICES_PATH);
	if (devices_dir == NULL) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	struct dirent* device_entry;
	while ((device_entry = readdir(devices_dir)) != NULL) {
		char path[512];
		snprintf(path, sizeof(path), SYSFS_DEVICES_PATH "/%s/descriptors", device_entry->d_name);
		int fd;
		fd = open(path, O_RDONLY);
		if (fd == -1) {
			continue;
		}
		struct usb_descriptor descriptors = {0};
		if (read(fd, &descriptors, sizeof(descriptors)) != sizeof(descriptors)) {
			close(fd);
			continue;
		}
		close(fd);
		const libredxx_find_filter* filter = libredxx_match_filter(descriptors.idVendor, descriptors.idProduct, filters, filters_count);
		if (filter) {
			snprintf(path, sizeof(path), SYSFS_DEVICES_PATH "/%s/busnum", device_entry->d_name);
			char busnum[4];
			if (libredxx_read_text_file(path, busnum, sizeof(busnum)) == -1) {
				continue; // this is a warning, the filter matched but we couldn't find the usbfs location
			}

			snprintf(path, sizeof(path), SYSFS_DEVICES_PATH "/%s/devnum", device_entry->d_name);
			char devnum[4];
			if (libredxx_read_text_file(path, devnum, sizeof(devnum)) == -1) {
				continue; // this is a warning, the filter matched but we couldn't find the usbfs location
			}

			private_devices = realloc(private_devices, sizeof(libredxx_found_device) * (device_index + 1));
			libredxx_found_device* private_device = &private_devices[device_index++];
			memset(private_device, 0, sizeof(libredxx_found_device));

			snprintf(private_device->path, sizeof(private_device->path), USBFS_PATH "/%03d/%03d", atoi(busnum), atoi(devnum));

			private_device->id = filter->id;
			private_device->type = filter->type;

			snprintf(path, sizeof(path), SYSFS_DEVICES_PATH "/%s/serial", device_entry->d_name);
			libredxx_read_text_file(path, private_device->serial.serial, sizeof(private_device->serial.serial));

			snprintf(path, sizeof(path), SYSFS_DEVICES_PATH "/%s/bNumInterfaces", device_entry->d_name);
			char interface_count[4] = {0};
			if (libredxx_read_text_file(path, interface_count, sizeof(interface_count)) != -1) {
				private_device->interface_count = atoi(interface_count);
			}
		}
	}
	closedir(devices_dir);
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

libredxx_status libredxx_get_serial(const libredxx_found_device* found, libredxx_serial* serial)
{
	memcpy(serial->serial, found->serial.serial, sizeof(serial->serial));
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
	for (unsigned int i = 0; i < found->interface_count; ++i) {
		if (ioctl(handle, USBDEVFS_CLAIMINTERFACE, &i) == -1) {
			close(handle);
			return LIBREDXX_STATUS_ERROR_SYS;
		}
	}
	libredxx_opened_device* private_opened = calloc(1, sizeof(libredxx_opened_device));
	if (!private_opened) {
		close(handle);
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	private_opened->found = *found;
	private_opened->handle = handle;
	if (found->type == LIBREDXX_DEVICE_TYPE_D3XX) {
		if (pipe(private_opened->d3xx_pipes) == -1) {
			free(private_opened);
			close(handle);
			return LIBREDXX_STATUS_ERROR_SYS;
		}
	} else {
		// wMaxPacketSize
		private_opened->d2xx_rx_buffer = malloc(512);
		private_opened->d2xx_rx_buffer_size = 512;
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
	for (unsigned int i = 0; i < device->found.interface_count; ++i) {
		ioctl(device->handle, USBDEVFS_RELEASEINTERFACE, &i);
	}
	close(device->handle);
	free(device);
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_interrupt(libredxx_opened_device* device)
{
	device->read_interrupted = true;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D3XX) {
		uint64_t one = 1;
		if (write(device->d3xx_pipes[1], &one, sizeof(one)) != sizeof(one)) {
			return LIBREDXX_STATUS_ERROR_SYS;
		}
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
		struct usbdevfs_bulktransfer bulk = {0};
		bulk.ep = 0x81;
		bulk.len = device->d2xx_rx_buffer_size;
		bulk.data = device->d2xx_rx_buffer;
		device->read_interrupted = false;
		while (true) {
			int r = ioctl(device->handle, USBDEVFS_BULK, &bulk);
			if (r == -1) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			if (r > D2XX_HEADER_SIZE) {
				*buffer_size = r - D2XX_HEADER_SIZE;
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
