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
#include "libredxx.h"

libredxx_status libredxx_ft260_i2c_write(libredxx_opened_device* device, uint8_t addr, const uint8_t* ctrl_buffer,
                                         const size_t* ctrl_buffer_size, const uint8_t* data_buffer, const size_t* data_buffer_size);
libredxx_status libredxx_ft260_i2c_read(libredxx_opened_device* device, uint8_t addr, uint8_t* ctrl_buffer, size_t* ctrl_buffer_size, uint8_t* buffer, size_t* buffer_size);

#endif //LIBREDXX_LIBREDXX_FT260_H
