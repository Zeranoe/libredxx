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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "libredxx_ft260.h"

#define FT260_I2C_WRITE_ID 0xDE

enum
{
    FT260_I2C_FLAG_NONE = 0x00,
    FT260_I2C_FLAG_START = 0x02,
    FT260_I2C_FLAG_REPEATED_START = 0x03,
    FT260_I2C_FLAG_STOP = 0x04,
};

#define FT260_I2C_WRITE_DATA_SIZE (LIBREDXX_FT260_REPORT_SIZE - sizeof(struct i2c_write))

#pragma pack(push, 1)

struct i2c_header
{
    uint8_t id;
    uint8_t addr;
    uint8_t flag;
};

struct i2c_write
{
    struct i2c_header header;
    uint8_t size;
    uint8_t data[];
};

struct i2c_read
{
    struct i2c_header header;
    uint16_t length;
};

#pragma pack(pop)

struct libredxx_i2c* libredxx_ft260_format_write(const uint8_t addr, const uint8_t* data, const size_t size) {
    if (!data || !size) {
        return NULL;
    }
    if (addr > (1 << 7) - 1)
    {
        // addresses must be 7-bit
        return NULL;
    }

    struct libredxx_i2c* head;
    struct libredxx_i2c* cur;
    size_t rem_size = size;
    while (rem_size)
    {
        struct libredxx_i2c* item = malloc(sizeof(struct libredxx_i2c));
        if (!item)
        {
            libredxx_ft260_free_i2c_items(head);
            return NULL;
        }
        const size_t transfer_size = rem_size > FT260_I2C_WRITE_DATA_SIZE
                                         ? FT260_I2C_WRITE_DATA_SIZE
                                         : rem_size;
        struct i2c_write* rep = (struct i2c_write*)item->data;
        rep->header.id = FT260_I2C_WRITE_ID;
        rep->header.addr = addr;
        rep->size = transfer_size;
        memcpy(rep->data, data + (size - rem_size), transfer_size);

        const bool first = rem_size == size;
        rem_size -= transfer_size;
        const bool last = rem_size == 0;

        rep->header.flag = FT260_I2C_FLAG_NONE;
        if (first)
        {
            rep->header.flag |= FT260_I2C_FLAG_START;
            head = cur = item;
        }
        else
        {
            cur->next = item;
            cur = item;
        }
        if (last)
        {
            rep->header.flag |= FT260_I2C_FLAG_STOP;
        }
    }
    cur->next = NULL;
    return head;
}

void libredxx_ft260_free_i2c_items(struct libredxx_i2c* item)
{
    while (item)
    {
        struct libredxx_i2c* e = item;
        item = item->next;
        free(e);
    }
}