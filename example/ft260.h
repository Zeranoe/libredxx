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

#if defined(__GNUC__)
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#else
#error unsupported compiler
#endif //LIBREDXX_LIBREDXX_PLATFORM_H

/*
 * Note not all reports are included. Consult the FT260 user guide.
 *
 * TODO add static assert for array sizes?
 */

PACK(struct libredxx_ft260_feature_out_report {
	union {
		struct {
			uint8_t report_id;
			union {
				struct {
					uint8_t request;
					uint8_t clk_ctl;
				} system_clock;
				struct {
					uint8_t request;
					uint8_t function;
				} gpio_function;
				struct {
					uint8_t request;
					uint8_t speed_lsb;
					uint8_t speed_msb;
				} i2c_clock_speed;
				struct {
					uint8_t gpio_val;
					uint8_t gpio_dir;
					uint8_t gpio_val_ex;
					uint8_t gpio_dir_ex;
				} gpio_write;
			};
		};
		uint8_t bytes[64];
	};
});

PACK(struct libredxx_ft260_feature_in_report {
	union {
		struct {
			uint8_t report_id;
			union {
				struct {
					uint8_t gpio_val;
					uint8_t gpio_dir;
					uint8_t gpio_val_ex;
					uint8_t gpio_dir_ex;
					uint8_t reserved[59];
				} gpio_read;
			};
		};
		uint8_t bytes[64];
	};
});

PACK(struct libredxx_ft260_output_report {
	union {
		struct {
			uint8_t report_id;
			union {
				struct {
					uint8_t slave_addr;
					uint8_t flags;
					uint8_t length;
					uint8_t data[60];
				} i2c_write_request;
				struct {
					uint8_t slave_addr;
					uint8_t flags;
					uint16_t length;
					uint8_t reserved[59];
				} i2c_read_request;
			};
		};
		uint8_t bytes[64];
	};
});

PACK(struct libredxx_ft260_input_report {
	union {
		struct {
			uint8_t report_id;
			union {
				struct {
					uint8_t length;
					uint8_t data[62];
				} i2c_read;
			};
		};
		uint8_t bytes[64];
	};
});

#endif //LIBREDXX_LIBREDXX_FT260_H
