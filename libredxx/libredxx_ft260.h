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

#ifndef LIBREDXX_LIBREDXX_FT260_H
#define LIBREDXX_LIBREDXX_FT260_H

#include <stdint.h>

#define LIBREDXX_FT260_REPORT_SIZE 64

/*
 * NOTE: not all reports are included. Consult the FT260 user guide.
 */

#pragma pack(push, 1)

struct libredxx_ft260_feature_out_i2c_reset {
	uint8_t report_id;
	uint8_t request;
	uint8_t reserved[62];
};

struct libredxx_ft260_feature_out_i2c_speed {
	uint8_t report_id;
	uint8_t request;
	uint8_t speed_lsb;
	uint8_t speed_msb;
	uint8_t reserved[60];
};

struct libredxx_ft260_feature_out_gpio_function {
	uint8_t report_id;
	uint8_t request;
	uint8_t function;
	uint8_t reserved[61];
};

struct libredxx_ft260_feature_out_gpio {
	uint8_t report_id;
	uint8_t gpio_val;
	uint8_t gpio_dir;
	uint8_t gpio_val_ex;
	uint8_t gpio_dir_ex;
	uint8_t reserved[59];
};

struct libredxx_ft260_feature_in_gpio {
	uint8_t report_id;
	uint8_t gpio_val;
	uint8_t gpio_dir;
	uint8_t gpio_val_ex;
	uint8_t gpio_dir_ex;
	uint8_t reserved[59];
};

struct libredxx_ft260_out_i2c_write {
	uint8_t report_id;
	uint8_t slave_addr;
	uint8_t flags;
	uint8_t length;
	uint8_t data[60];
};

struct libredxx_ft260_out_i2c_read {
	uint8_t report_id;
	uint8_t slave_addr;
	uint8_t flags;
	uint8_t length;
	uint8_t data[60];
};

struct libredxx_ft260_in_i2c_read {
	uint8_t report_id;
	uint8_t length;
	uint8_t data[62];
};

#pragma pack(pop)

#endif //LIBREDXX_LIBREDXX_FT260_H
