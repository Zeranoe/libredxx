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
#include <stddef.h>
#include <stdint.h>

#define LIBREDXX_FT260_REPORT_SIZE 64

struct libredxx_i2c
{
    uint8_t data[LIBREDXX_FT260_REPORT_SIZE];
    struct libredxx_i2c* next;
};

struct libredxx_i2c* libredxx_ft260_format_write(uint8_t addr, const uint8_t* data, size_t size);
void libredxx_ft260_free_i2c_items(struct libredxx_i2c* item);

#endif //LIBREDXX_LIBREDXX_FT260_H
