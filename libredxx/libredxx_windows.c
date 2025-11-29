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

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>
#include <devpkey.h>
#include <hidsdi.h>
#include <winioctl.h>
#include <hidclass.h>

struct libredxx_found_device {
	WCHAR path[256];
	libredxx_serial serial;
	libredxx_device_id id;
	libredxx_device_type type;
};

struct libredxx_opened_device {
	libredxx_found_device found;
	HANDLE handle;
	HANDLE d2xx_read_event;
	size_t d3xx_stream_pipe;
	bool read_interrupted;
};

static int32_t find_last_of(const wchar_t* str, uint32_t str_len, wchar_t needle)
{
	for (uint32_t i = str_len; i > 0; --i) {
		if (str[i] == needle) {
			return i;
		}
	}
	return -1;
}

libredxx_status libredxx_enumerate_interfaces(HDEVINFO dev_info, const libredxx_find_filter* filters, size_t filters_count, libredxx_found_device*** devices, size_t* devices_count)
{
	GUID guids[] = {
		{0x219D0508, 0x57A8, 0x4FF5, {0x97, 0xA1, 0xBD, 0x86, 0x58, 0x7C, 0x6C, 0x7E}}, // D2XX
		{0xD1E8FE6A, 0xAB75, 0x4D9E, {0x97, 0xD2, 0x06, 0xFA, 0x22, 0xC7, 0x73, 0x6C}}, // D3XX
		GUID_DEVINTERFACE_HID,
	};
	size_t device_index = 0;
	size_t guids_count = sizeof(guids) / sizeof(guids[0]);
	libredxx_found_device* private_devices = NULL;
	for (size_t guid_index = 0; guid_index < guids_count; ++guid_index) {
		GUID* guid = &guids[guid_index];
		DWORD member_index = 0;
		while (1) {
			SP_DEVICE_INTERFACE_DATA ifd;
			ifd.cbSize = sizeof(ifd);
			if (!SetupDiEnumDeviceInterfaces(dev_info, NULL, guid, member_index++, &ifd)) {
				DWORD err = GetLastError();
				if (err == ERROR_NO_MORE_ITEMS) {
					break;
				}
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			uint8_t detail_buffer[512];
			SP_DEVICE_INTERFACE_DETAIL_DATA* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA*)detail_buffer;
			detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			SP_DEVINFO_DATA did;
			did.cbSize = sizeof(did);
			if (!SetupDiGetDeviceInterfaceDetailW(dev_info, &ifd, detail, sizeof(detail_buffer), NULL, &did)) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			wchar_t* vid_start = wcsstr(detail->DevicePath, L"vid_");
			if (!vid_start) {
				continue;
			}
			vid_start += 4;
			wchar_t* pid_start = wcsstr(detail->DevicePath, L"pid_");
			if (!pid_start) {
				continue;
			}
			pid_start += 4;
			wchar_t vid_str[5] = {vid_start[0], vid_start[1], vid_start[2], vid_start[3], '\0'};
			wchar_t pid_str[5] = {pid_start[0], pid_start[1], pid_start[2], pid_start[3], '\0'};
			uint16_t vid = (uint16_t)wcstol(vid_str, NULL, 16);
			uint16_t pid = (uint16_t)wcstol(pid_str, NULL, 16);
			for (size_t filter_index = 0; filter_index < filters_count; ++filter_index) {
				const libredxx_find_filter* filter = &filters[filter_index];
				if (filter->id.vid == vid && filter->id.pid == pid) {
					DEVPROPTYPE ptype;
					DWORD prop_size;
					wchar_t* serial = NULL;

					wchar_t inst[256];
					if (!SetupDiGetDevicePropertyW(dev_info, &did, &DEVPKEY_Device_InstanceId, &ptype, (PBYTE)inst, sizeof(inst), &prop_size, 0)) {
						break;
					}
					const uint32_t inst_len = prop_size / sizeof(wchar_t);

					// get the parent, every device should have a parent
					wchar_t parent[256];
					if (!SetupDiGetDevicePropertyW(dev_info, &did, &DEVPKEY_Device_Parent, &ptype, (PBYTE)parent, sizeof(parent), &prop_size, 0)) {
						break;
					}
					const uint32_t parent_len = prop_size / sizeof(wchar_t);
					wchar_t* parent_vid_start = wcsstr(parent, L"VID_");
					wchar_t* parent_pid_start = wcsstr(parent, L"PID_");
					if (parent_vid_start && parent_pid_start) {
						parent_vid_start += 4;
						parent_pid_start += 4;
						wchar_t parent_vid_str[5] = {parent_vid_start[0], parent_vid_start[1], parent_vid_start[2], parent_vid_start[3], '\0'};
						wchar_t parent_pid_str[5] = {parent_pid_start[0], parent_pid_start[1], parent_pid_start[2], parent_pid_start[3], '\0'};
						uint16_t parent_vid = (uint16_t)wcstol(parent_vid_str, NULL, 16);
						uint16_t parent_pid = (uint16_t)wcstol(parent_pid_str, NULL, 16);
						if (parent_vid == vid && parent_pid == pid) {
							int32_t channel_offset = find_last_of(inst, inst_len, L'&');
							if (channel_offset == -1) {
								break; // not a valid multi-channel device with a parent
							}
							++channel_offset; // move past &
							uint16_t channel_number = (uint16_t)wcstol(&inst[channel_offset], NULL, 16);
							if (channel_number > 0) {
								break; // TODO: support multi-channel
							}
							int32_t serial_offset = find_last_of(parent, parent_len, L'\\');
							if (serial_offset != -1) {
								++serial_offset; // move past backslash
								serial = &parent[serial_offset];
							}
						}
					}
					if (!serial) {
						// no serial found in the parent, must be on this interface
						int32_t serial_offset = find_last_of(inst, inst_len, L'\\');
						if (serial_offset != -1) {
							++serial_offset; // move past backslash
							serial = &inst[serial_offset];
						}
					}
					private_devices = realloc(private_devices, sizeof(libredxx_found_device) * (device_index + 1));
					libredxx_found_device* device = &private_devices[device_index++];
					memset(device, 0, sizeof(libredxx_found_device));
					device->id.vid = vid;
					device->id.pid = pid;
					wcscpy_s(device->path, sizeof(device->path) / sizeof(device->path[0]), detail->DevicePath);
					device->type = filter->type;
					if (serial) {
						WideCharToMultiByte(CP_UTF8, 0, serial, -1, device->serial.serial, sizeof(device->serial.serial), NULL, NULL);
					}
					break;
				}
			}
		}
	}
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

libredxx_status libredxx_find_devices(const libredxx_find_filter* filters, size_t filters_count, libredxx_found_device*** devices, size_t* devices_count)
{
	libredxx_status status;
	HDEVINFO dev_info = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (dev_info == INVALID_HANDLE_VALUE) {
		status = LIBREDXX_STATUS_ERROR_SYS;
	} else {
		status = libredxx_enumerate_interfaces(dev_info, filters, filters_count, devices, devices_count);
	}
	SetupDiDestroyDeviceInfoList(dev_info);
	return status;
}

libredxx_status libredxx_free_found(libredxx_found_device** devices)
{
	if (!devices) {
		return LIBREDXX_STATUS_SUCCESS;
	}
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

static libredxx_status libredxx_d3xx_set_timeout(libredxx_opened_device* device, uint8_t pipe, uint32_t timeout)
{
	uint8_t* timeout_bytes = (uint8_t*)&timeout;
	uint8_t in[8] = {
		timeout_bytes[0],
		timeout_bytes[1],
		timeout_bytes[2],
		timeout_bytes[3],
		pipe,
		0x00,
		0x00,
		0x00
	};
	if (!DeviceIoControl(device->handle, 0x0022227C, in, sizeof(in), NULL, 0, NULL, NULL)) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_open_device(const libredxx_found_device* found, libredxx_opened_device** opened)
{
	DWORD create_flags = found->type == LIBREDXX_DEVICE_TYPE_D2XX ? 0 : FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL;
	HANDLE handle = CreateFileW(found->path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, create_flags, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	libredxx_opened_device* private_opened = malloc(sizeof(libredxx_opened_device));
	if (!private_opened) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	private_opened->found = *found;
	private_opened->handle = handle;
	private_opened->d2xx_read_event = NULL;
	private_opened->d3xx_stream_pipe = 0;
	private_opened->read_interrupted = false;
	if (found->type == LIBREDXX_DEVICE_TYPE_D3XX) {
		libredxx_status status;
		// disable timeouts
		status = libredxx_d3xx_set_timeout(private_opened, 0x02, 0);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			free(private_opened);
			return status;
		}
		status = libredxx_d3xx_set_timeout(private_opened, 0x82, 0);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			free(private_opened);
			return status;
		}
	}
	*opened = private_opened;
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_close_device(libredxx_opened_device* device)
{
	libredxx_interrupt(device);
	CloseHandle(device->handle);
	device->handle = NULL;
	free(device);
	return LIBREDXX_STATUS_SUCCESS;
}

static libredxx_status libredxx_d3xx_abort_pipe(libredxx_opened_device* device, uint8_t pipe)
{
	if (!DeviceIoControl(device->handle, 0x00222298, &pipe, sizeof(pipe), NULL, 0, NULL, NULL)) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	return LIBREDXX_STATUS_SUCCESS;
}

static libredxx_status libredxx_d3xx_set_stream_pipe(libredxx_opened_device* device, uint8_t pipe, size_t size)
{
	uint8_t* size_bytes = (uint8_t*)&size;
	uint8_t arg[12] = {
		0x00,
		0x00,
		0x00,
		0x00,
		size_bytes[0],
		size_bytes[1],
		size_bytes[2],
		size_bytes[3],
		pipe,
		0x00,
		0x00,
		0x00
	};
	if (!DeviceIoControl(device->handle, 0x0022221C, arg, sizeof(arg), NULL, 0, NULL, NULL)) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_interrupt(libredxx_opened_device* device)
{
	device->read_interrupted = true;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D2XX) {
		return SetEvent(device->d2xx_read_event) ? LIBREDXX_STATUS_SUCCESS : LIBREDXX_STATUS_ERROR_SYS;
	} else if (device->found.type == LIBREDXX_DEVICE_TYPE_D3XX) {
		// abort also released the overlapped event
		libredxx_status status;
		status = libredxx_d3xx_abort_pipe(device, 0x82);
		if (status != LIBREDXX_STATUS_SUCCESS) {
			return status;
		}
		status = libredxx_d3xx_abort_pipe(device, 0x02);
		return status;
	} else if (device->found.type == LIBREDXX_DEVICE_TYPE_FT260) {
		if (!CancelIoEx(device->handle, NULL)) {
			return LIBREDXX_STATUS_ERROR_SYS;
		}
		return LIBREDXX_STATUS_SUCCESS;
	} else {
		return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
	}
}

static libredxx_status libredxx_d2xx_rx_available(libredxx_opened_device* device, size_t* available)
{
	DWORD available_dw;
	DWORD available_dw_size = sizeof(available_dw);
	if (!DeviceIoControl(device->handle, 0x0022216C, &available_dw, available_dw_size, &available_dw, available_dw_size, &available_dw_size, NULL)) {
		return LIBREDXX_STATUS_ERROR_SYS;
	}
	*available = available_dw;
	return LIBREDXX_STATUS_SUCCESS;
}

static libredxx_status libredxx_d2xx_wait_rx(libredxx_opened_device* device, size_t* available)
{
	libredxx_status ret = LIBREDXX_STATUS_SUCCESS;
	device->d2xx_read_event = CreateEventW(NULL, false, false, NULL);
	uint8_t* device_addr_bytes = (uint8_t*)&device;
	uint8_t* rx_event_addr_bytes = (uint8_t*)&device->d2xx_read_event;
	uint8_t req[16] = {
		device_addr_bytes[0],
		device_addr_bytes[1],
		device_addr_bytes[2],
		device_addr_bytes[3],
		0x01, // event id
		0x00,
		0x00,
		0x00,
		rx_event_addr_bytes[0],
		rx_event_addr_bytes[1],
		rx_event_addr_bytes[2],
		rx_event_addr_bytes[3],
		rx_event_addr_bytes[4],
		rx_event_addr_bytes[5],
		rx_event_addr_bytes[6],
		rx_event_addr_bytes[7]
	};
	if (!DeviceIoControl(device->handle, 0x0022208C, &req, sizeof(req), NULL, 0, NULL, NULL)) {
		ret = LIBREDXX_STATUS_ERROR_SYS;
	} else {
		// Now that the event is registered, we can check if there's any pending data. If we check before registering the event, we could miss the event
		ret = libredxx_d2xx_rx_available(device, available);
		if (ret == LIBREDXX_STATUS_SUCCESS && *available == 0) {
			device->read_interrupted = false;
			if (WaitForSingleObject(device->d2xx_read_event, INFINITE) != WAIT_OBJECT_0) {
				ret = LIBREDXX_STATUS_ERROR_SYS;
			} else if (device->read_interrupted) {
				ret = LIBREDXX_STATUS_ERROR_INTERRUPTED;
			} else {
				ret = libredxx_d2xx_rx_available(device, available);
			}
		}
	}
	device->d2xx_read_event = NULL;
	CloseHandle(device->d2xx_read_event);
	return ret;
}

libredxx_status libredxx_read(libredxx_opened_device* device, void* buffer, size_t* buffer_size, libredxx_endpoint endpoint)
{
	BYTE* bBuffer = (BYTE*)buffer;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D2XX) {
		if (endpoint == LIBREDXX_ENDPOINT_IO) {
			libredxx_status status;
			size_t available;
			status = libredxx_d2xx_wait_rx(device, &available);
			if (status != LIBREDXX_STATUS_SUCCESS) {
				return status;
			}

			*buffer_size = min(available, *buffer_size);
			if (!ReadFile(device->handle, buffer, (DWORD)*buffer_size, (DWORD*)buffer_size, NULL)) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			return LIBREDXX_STATUS_SUCCESS;
		} else {
			return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
		}
	} else if (device->found.type == LIBREDXX_DEVICE_TYPE_D3XX) {
		if (endpoint == LIBREDXX_ENDPOINT_IO) {
			libredxx_status ret = LIBREDXX_STATUS_SUCCESS;
			uint8_t read_pipe = 0x82;
			if (device->d3xx_stream_pipe != *buffer_size) {
				ret = libredxx_d3xx_set_stream_pipe(device, read_pipe, *buffer_size);
				if (ret != LIBREDXX_STATUS_SUCCESS) {
					return ret;
				}
				device->d3xx_stream_pipe = *buffer_size;
			}
			OVERLAPPED overlapped = {0};
			overlapped.hEvent = CreateEventW(NULL, true, false, NULL);
			if (!DeviceIoControl(device->handle, 0x0022220A, &read_pipe, sizeof(read_pipe), (DWORD*)buffer, (DWORD)*buffer_size, NULL, &overlapped)) {
				if (GetLastError() != ERROR_IO_PENDING) {
					ret = LIBREDXX_STATUS_ERROR_SYS;
				} else {
					device->read_interrupted = false;
					DWORD transferred = 0;
					if (!GetOverlappedResult(device->handle, &overlapped, &transferred, true)) {
						ret = (GetLastError() == ERROR_OPERATION_ABORTED && device->read_interrupted) ? LIBREDXX_STATUS_ERROR_INTERRUPTED : LIBREDXX_STATUS_ERROR_SYS;
					}
					*buffer_size = transferred;
				}
			}
			CloseHandle(overlapped.hEvent);
			device->d2xx_read_event = NULL;
			return ret;
		} else {
			return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
		}
	} else if (device->found.type == LIBREDXX_DEVICE_TYPE_FT260) {
		if (endpoint == LIBREDXX_ENDPOINT_CONTROL) {
			const BYTE report_id = bBuffer[0];
			if (!report_id || *buffer_size != LIBREDXX_FT260_REPORT_SIZE) {
				return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
			}
			DWORD bytes_returned = 0;
			// For GET_FEATURE, pass the report ID in the input buffer (size 1),
			// and receive the full feature report into the output buffer.
			if (!DeviceIoControl(device->handle,
							 IOCTL_HID_GET_FEATURE,
							 buffer,
							 1,
							 buffer,
							 (DWORD)*buffer_size,
							 &bytes_returned,
							 NULL)) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			*buffer_size = bytes_returned;
			return LIBREDXX_STATUS_SUCCESS;
		} else if (endpoint == LIBREDXX_ENDPOINT_IO) {
			libredxx_status ret = LIBREDXX_STATUS_SUCCESS;
			OVERLAPPED overlapped = {0};
			overlapped.hEvent = CreateEventW(NULL, true, false, NULL);
			if (!ReadFile(device->handle, buffer, (DWORD)*buffer_size, (DWORD*)buffer_size, &overlapped)) {
				if (GetLastError() != ERROR_IO_PENDING) {
					ret = LIBREDXX_STATUS_ERROR_SYS;
				} else if (!GetOverlappedResult(device->handle, &overlapped, (DWORD*)buffer_size, true)) {
					ret = (GetLastError() == ERROR_OPERATION_ABORTED && device->read_interrupted) ? LIBREDXX_STATUS_ERROR_INTERRUPTED : LIBREDXX_STATUS_ERROR_SYS;
				}
			}
			CloseHandle(overlapped.hEvent);
			return ret;
		} else {
			return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
		}
	} else {
		return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
	}
}

libredxx_status libredxx_write(libredxx_opened_device* device, void* buffer, size_t* buffer_size, libredxx_endpoint endpoint)
{
	BYTE* bBuffer = (BYTE*)buffer;
	if (device->found.type == LIBREDXX_DEVICE_TYPE_D2XX) {
		if (endpoint == LIBREDXX_ENDPOINT_IO) {
			if (!WriteFile(device->handle, buffer, (DWORD)*buffer_size, (DWORD*)buffer_size, NULL)) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			return LIBREDXX_STATUS_SUCCESS;
		} else {
			return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
		}
	} else if (device->found.type == LIBREDXX_DEVICE_TYPE_D3XX) {
		if (endpoint == LIBREDXX_ENDPOINT_IO) {
			libredxx_status ret = LIBREDXX_STATUS_SUCCESS;
			OVERLAPPED overlapped = {0};
			overlapped.hEvent = CreateEventW(NULL, true, false, NULL);
			uint8_t write_pipe = 0x02;
			if (!DeviceIoControl(device->handle, 0x0022220D, &write_pipe, sizeof(write_pipe), (DWORD*)buffer, (DWORD)*buffer_size, NULL, &overlapped)) {
				if (GetLastError() != ERROR_IO_PENDING) {
					ret = LIBREDXX_STATUS_ERROR_SYS;
				} else {
					if (!GetOverlappedResult(device->handle, &overlapped, (DWORD*)buffer_size, true)) {
						ret = LIBREDXX_STATUS_ERROR_SYS;
					}
				}
			}
			CloseHandle(overlapped.hEvent);
			return ret;
		} else {
			return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
		}
	} else if (device->found.type == LIBREDXX_DEVICE_TYPE_FT260) {
		const BYTE report_id = bBuffer[0];
		if (!report_id) {
			return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
		}
		if (endpoint == LIBREDXX_ENDPOINT_CONTROL) {
			if (*buffer_size != LIBREDXX_FT260_REPORT_SIZE) {
				return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
			}
			DWORD bytes_returned = 0;
			if (!DeviceIoControl(device->handle,
							 IOCTL_HID_SET_FEATURE,
							 buffer,
							 (DWORD)*buffer_size,
							 NULL,
							 0,
							 &bytes_returned,
							 NULL)) {
				return LIBREDXX_STATUS_ERROR_SYS;
			}
			return LIBREDXX_STATUS_SUCCESS;
		} else if (endpoint == LIBREDXX_ENDPOINT_IO) {
			libredxx_status ret = LIBREDXX_STATUS_SUCCESS;
			OVERLAPPED overlapped = {0};
			overlapped.hEvent = CreateEventW(NULL, true, false, NULL);
			if (!WriteFile(device->handle, buffer, (DWORD)*buffer_size, (DWORD*)buffer_size, &overlapped)) {
				if (GetLastError() != ERROR_IO_PENDING) {
					ret = LIBREDXX_STATUS_ERROR_SYS;
				} else if (!GetOverlappedResult(device->handle, &overlapped, (DWORD*)buffer_size, true)) {
					ret = LIBREDXX_STATUS_ERROR_SYS;
				}
			}
			CloseHandle(overlapped.hEvent);
			return ret;
		} else {
			return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
		}
	} else {
		return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
	}
}
