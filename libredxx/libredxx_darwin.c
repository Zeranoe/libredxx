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

#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>

#define D2XX_HEADER_SIZE 2

// for details: https://developer.apple.com/library/archive/documentation/DeviceDrivers/Conceptual/USBBook/USBDeviceInterfaces/USBDevInterfaces.html

struct libredxx_found_device {
	libredxx_device_id id;
	libredxx_device_type type;
	uint32_t location;
};

struct libredxx_opened_device {
	libredxx_found_device found;
	IOUSBDeviceInterface** device;
	IOUSBInterfaceInterface** interfaces[2];
	uint8_t* d2xx_rx_buffer;
	size_t d2xx_rx_buffer_size;
	bool read_interrupted;
};

libredxx_status libredxx_find_devices(const libredxx_find_filter* filters, size_t filters_count, libredxx_found_device*** devices, size_t* devices_count)
{
	io_iterator_t device_it;
	{
		CFMutableDictionaryRef dict = IOServiceMatching(kIOUSBDeviceClassName);
		IOServiceGetMatchingServices(0, dict, &device_it);
	}
	libredxx_found_device* private_devices = NULL;
	io_service_t darwin_device_service;
	size_t device_index = 0;
	while ((darwin_device_service = IOIteratorNext(device_it))) {
		IOUSBDeviceInterface** darwin_device = NULL;
		IOCFPlugInInterface** plug_in_interface = NULL; // reused for the device and interface(s)
		{
			SInt32 score;
			IOCreatePlugInInterfaceForService(darwin_device_service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plug_in_interface, &score);
			IOObjectRelease(darwin_device_service);
			(*plug_in_interface)->QueryInterface(plug_in_interface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&darwin_device);
			(*plug_in_interface)->Release(plug_in_interface);
		}

		uint16_t vid;
		uint16_t pid;
		(*darwin_device)->GetDeviceVendor(darwin_device, &vid);
		(*darwin_device)->GetDeviceProduct(darwin_device, &pid);
		for (size_t filter_index = 0; filter_index < filters_count; ++filter_index) {
			const libredxx_find_filter* filter = &filters[filter_index];
			if (vid == filter->id.vid && pid == filter->id.pid) {
				private_devices = realloc(private_devices, sizeof(libredxx_found_device) * (device_index + 1));
				libredxx_found_device* device = &private_devices[device_index];
				device->id.vid = vid;
				device->id.pid = pid;
				device->type = filter->type;

				(*darwin_device)->GetLocationID(darwin_device, &device->location);

				++device_index;
				break;
			}
		}
		(*darwin_device)->Release(darwin_device);
	}
	IOObjectRelease(device_it);
	*devices_count = device_index;
	*devices = NULL;
	if (*devices_count > 0) {
		*devices = malloc(sizeof(libredxx_found_device*) * *devices_count);
		for (size_t i = 0; i < *devices_count; ++i) {
			(*devices)[i] = &private_devices[i];
		}
	}
	return LIBREDXX_STATUS_SUCCESS;
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
	libredxx_opened_device* private_device = calloc(1, sizeof(libredxx_opened_device));
	private_device->found = *found;

	io_iterator_t interface_iterator;
	{ // all to just get an iterator to the interfaces
		io_service_t device_service;
		{
			CFMutableDictionaryRef matching_dict = IOServiceMatching(kIOUSBDeviceClassName);
			CFMutableDictionaryRef prop_matching_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFTypeRef location_cf = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &found->location);
			CFDictionarySetValue(prop_matching_dict, CFSTR(kUSBDevicePropertyLocationID), location_cf);
			CFDictionarySetValue(matching_dict, CFSTR(kIOPropertyMatchKey), prop_matching_dict);
			CFRelease(prop_matching_dict);
			CFRelease(location_cf);
			io_iterator_t device_it;
			IOServiceGetMatchingServices(0, matching_dict, &device_it);
			device_service = IOIteratorNext(device_it); // there should only be one...
			IOObjectRelease(device_it);
		}

		IOUSBDeviceInterface** darwin_device = NULL;
		{
			IOCFPlugInInterface** plug_in_interface = NULL;
			SInt32 score;
			IOCreatePlugInInterfaceForService(device_service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plug_in_interface, &score);
			IOObjectRelease(device_service);
			(*plug_in_interface)->QueryInterface(plug_in_interface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID*)&darwin_device);
			(*plug_in_interface)->Release(plug_in_interface);
		}
		(*darwin_device)->USBDeviceOpenSeize(darwin_device);
		private_device->device = darwin_device;

		IOUSBFindInterfaceRequest request;
		request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
		request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
		request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
		request.bAlternateSetting = kIOUSBFindInterfaceDontCare;

		(*darwin_device)->CreateInterfaceIterator(darwin_device, &request, &interface_iterator);
	}

	io_service_t interface_service;
	size_t interface_index = 0;
	while ((interface_service = IOIteratorNext(interface_iterator))) {
		IOCFPlugInInterface** plug_in_interface = NULL;
		SInt32 score;
		IOCreatePlugInInterfaceForService(interface_service, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plug_in_interface, &score);
		IOObjectRelease(interface_service);

		IOUSBInterfaceInterface** interface = NULL;
		(*plug_in_interface)->QueryInterface(plug_in_interface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *)&interface);
		(*plug_in_interface)->Release(plug_in_interface);
		(*interface)->USBInterfaceOpen(interface);
		private_device->interfaces[interface_index] = interface;
		++interface_index;
	}

	*opened = private_device;
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_close_device(libredxx_opened_device* device)
{
	libredxx_status status;
	status = libredxx_interrupt(device);
	if (status != LIBREDXX_STATUS_SUCCESS) {
		return status;
	}
	for (size_t i = 0; i < sizeof(device->interfaces); ++i) {
		IOUSBInterfaceInterface** interface = device->interfaces[i];
		if (!interface) {
			break;
		}
		(*interface)->USBInterfaceClose(interface);
		(*interface)->Release(interface);
	}
	(*device->device)->USBDeviceClose(device->device);
	(*device->device)->Release(device->device);
	free(device->d2xx_rx_buffer);
	free(device);
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_get_serial(const libredxx_opened_device* device, libredxx_serial* serial)
{
	uint8_t raw[255] = {0};
	IOUSBDevRequest req = {0};
	req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
	req.bRequest = kUSBRqGetDescriptor;
	req.wValue = (kUSBStringDesc << 8) | 3; // serial
	req.wIndex = 0x0409; // English
	req.wLength = sizeof(raw);
	req.pData = raw;
	IOReturn ret = (*device->device)->DeviceRequest(device->device, &req);
	if (ret != kIOReturnSuccess) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	if (req.wLenDone < 2 /* size (u8) + type (u8) */) {
		return LIBREDXX_STATUS_ERROR_IO;
	}
	if (raw[1] != kUSBStringDesc) {
		return LIBREDXX_STATUS_ERROR_IO;
	}
	// TODO: make sure we have enough room in serial to write this
	char* out = serial->serial;
	for (size_t i = 2; i < req.wLenDone; i += 2) {
		*out++ = raw[i];
	}
	*out = '\0';
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_interrupt(libredxx_opened_device* device)
{
	device->read_interrupted = true;
	size_t interface_index;
	uint8_t pipe;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D2XX) {
		interface_index = 0;
		pipe = 1;
	} else {
		interface_index = 1;
		pipe = 2;
	}
	IOUSBInterfaceInterface** interface = device->interfaces[interface_index];
	(*interface)->AbortPipe(interface, pipe);
	return LIBREDXX_STATUS_SUCCESS;
}

static libredxx_status libredxx_d3xx_trigger_read(libredxx_opened_device* device, uint32_t size)
{
	uint8_t* size_bytes = (uint8_t*)&size;
	uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x82, 0x01, 0x00, 0x00, size_bytes[0], size_bytes[1], size_bytes[2], size_bytes[3], 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	IOUSBInterfaceInterface** interface = (IOUSBInterfaceInterface**)device->interfaces[0];
	return (*interface)->WritePipe(interface, 0x01, data, sizeof(data)) == kIOReturnSuccess ? LIBREDXX_STATUS_SUCCESS : LIBREDXX_STATUS_ERROR_SYS;
}

libredxx_status libredxx_read(libredxx_opened_device* device, void* buffer, size_t* buffer_size)
{
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D2XX) {
		size_t headered_buffer_size = *buffer_size + D2XX_HEADER_SIZE;
		if (headered_buffer_size > device->d2xx_rx_buffer_size) {
			device->d2xx_rx_buffer = realloc(device->d2xx_rx_buffer, headered_buffer_size);
			device->d2xx_rx_buffer_size = headered_buffer_size;
		}
		IOUSBInterfaceInterface** interface = device->interfaces[0];
		device->read_interrupted = false;
		while (true) {
			UInt32 size = headered_buffer_size;
			IOReturn ret = (*interface)->ReadPipe(interface, 1, device->d2xx_rx_buffer, &size);
			if (ret != kIOReturnSuccess) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			if (size > D2XX_HEADER_SIZE) {
				size -= D2XX_HEADER_SIZE;
				memcpy(buffer, &device->d2xx_rx_buffer[D2XX_HEADER_SIZE], size);
				*buffer_size = size;
				return LIBREDXX_STATUS_SUCCESS;
			}
			if (device->read_interrupted) {
				return LIBREDXX_STATUS_ERROR_INTERRUPTED;
			}
		}
	} else {
		IOUSBInterfaceInterface** interface = (IOUSBInterfaceInterface**)device->interfaces[1];
		libredxx_status status;
		device->read_interrupted = false;
		status = libredxx_d3xx_trigger_read(device, *buffer_size);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			return status;
		}
		IOReturn ret = (*interface)->ReadPipe(interface, 2, buffer, (UInt32*)buffer_size);
		if (ret == kIOUSBTransactionReturned && device->read_interrupted) {
			return LIBREDXX_STATUS_ERROR_INTERRUPTED;
		}
		return ret == kIOReturnSuccess ? LIBREDXX_STATUS_SUCCESS : LIBREDXX_STATUS_ERROR_SYS;
	}
}

libredxx_status libredxx_write(libredxx_opened_device* device, void* buffer, size_t* buffer_size)
{
	size_t interface_index;
	uint8_t pipe;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D2XX) {
		interface_index = 0;
		pipe = 2;
	} else {
		interface_index = 1;
		pipe = 1;
	}
	IOUSBInterfaceInterface** interface = (IOUSBInterfaceInterface**)device->interfaces[interface_index];
	return (*interface)->WritePipe(interface, pipe, buffer, *buffer_size) == kIOReturnSuccess ? LIBREDXX_STATUS_SUCCESS : LIBREDXX_STATUS_ERROR_SYS;
}
