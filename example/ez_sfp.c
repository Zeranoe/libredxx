#include <string.h>
#include "libredxx/libredxx.h"
#include "ft260.h"

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
	libredxx_opened_device* opened = NULL;
	status = libredxx_open_device(found_devices[0], &opened);

	struct libredxx_ft260_feature_out_report rep_ftr_out;

	// set GPIO G function
	memset(&rep_ftr_out, 0, sizeof(rep_ftr_out));
	rep_ftr_out.report_id = 0xA1;
	rep_ftr_out.gpio_function.request = 0x09;
	rep_ftr_out.gpio_function.function = 0;



}