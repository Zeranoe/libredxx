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
#include <string.h>
#include "libredxx_ft260.h"

#define FT260_REPORT_SIZE 64
#define FT260_I2C_WRITE_DATA_SIZE (FT260_REPORT_SIZE - sizeof(struct i2c_write))
#define FT260_I2C_MAX_ADDR ((1 << 7) - 1)

enum
{
    FT260_I2C_READ_ID = 0xC2,
    FT260_I2C_WRITE_ID = 0xDE,
};

enum
{
    FT260_I2C_FLAG_NONE = 0x00,
    FT260_I2C_FLAG_START = 0x02,
    FT260_I2C_FLAG_REPEATED_START = 0x03,
    FT260_I2C_FLAG_STOP = 0x04,
};


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

libredxx_status libredxx_ft260_i2c_write(libredxx_opened_device* device, const uint8_t addr, const uint8_t* ctrl_buffer,
                                         const size_t* ctrl_buffer_size, const uint8_t* data_buffer, const size_t* data_buffer_size)
{
    if (addr > FT260_I2C_MAX_ADDR
        || (ctrl_buffer && !ctrl_buffer_size)
        || (!ctrl_buffer && ctrl_buffer_size)
        || (data_buffer && !data_buffer_size)
        || (!data_buffer && data_buffer_size))
    {
        return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
    }

    uint8_t buf[FT260_REPORT_SIZE];
    struct i2c_write* rep = (struct i2c_write*)buf;

    rep->header.flag = FT260_I2C_FLAG_START;

    size_t rem_ctrl_size = ctrl_buffer_size ? *ctrl_buffer_size : 0;
    size_t rem_data_size = data_buffer_size ? *data_buffer_size : 0;
    while (rem_ctrl_size || rem_data_size)
    {
        size_t transfer_size = 0;
        if (rem_ctrl_size)
        {
            const size_t ctrl_transfer_size = rem_ctrl_size > FT260_I2C_WRITE_DATA_SIZE
                                                  ? FT260_I2C_WRITE_DATA_SIZE
                                                  : rem_ctrl_size;
            memcpy(rep->data, ctrl_buffer + (*ctrl_buffer_size - rem_ctrl_size), ctrl_transfer_size);
            rem_ctrl_size -= ctrl_transfer_size;
            transfer_size += ctrl_transfer_size;
        }
        if (rem_data_size)
        {
            const size_t rem_transfer_size = FT260_I2C_WRITE_DATA_SIZE - transfer_size;
            const size_t data_transfer_size = rem_data_size > rem_transfer_size
                                         ? rem_transfer_size
                                         : rem_data_size;
            memcpy(rep->data + transfer_size, data_buffer + (*data_buffer_size - rem_data_size), data_transfer_size);
            rem_data_size -= data_transfer_size;
            transfer_size += data_transfer_size;
        }
        rep->header.id = FT260_I2C_WRITE_ID;
        rep->header.addr = addr;
        rep->size = transfer_size;

        if (!rem_ctrl_size && !rem_data_size)
        {
            rep->header.flag |= FT260_I2C_FLAG_STOP;
        }

        size_t buf_size = sizeof(buf);
        const libredxx_status status = libredxx_write(device, buf, &buf_size);
        if (status != LIBREDXX_STATUS_SUCCESS)
        {
            return status;
        }
        memset(buf, 0, sizeof(struct i2c_header));
    }
    return LIBREDXX_STATUS_SUCCESS;
}

libredxx_status libredxx_ft260_i2c_read(libredxx_opened_device* device, const uint8_t addr, uint8_t* ctrl_buffer, size_t* ctrl_buffer_size, uint8_t* buffer, size_t* buffer_size)
{
    if (addr > FT260_I2C_MAX_ADDR
        || (ctrl_buffer && !ctrl_buffer_size)
        || (!ctrl_buffer && ctrl_buffer_size)
        || !buffer || !buffer_size || !*buffer_size || *buffer_size > UINT16_MAX)
    {
        return LIBREDXX_STATUS_ERROR_INVALID_ARGUMENT;
    }
    libredxx_status status = libredxx_ft260_i2c_write(device, addr, ctrl_buffer, ctrl_buffer_size, NULL, NULL);
    if (status != LIBREDXX_STATUS_SUCCESS)
    {
        return status;
    }
    uint8_t buf[FT260_REPORT_SIZE];
    struct i2c_read* rep = (struct i2c_read*)buf;
    rep->header.id = FT260_I2C_READ_ID;
    rep->header.addr = addr;
    rep->header.flag = FT260_I2C_FLAG_START | FT260_I2C_FLAG_STOP;
    rep->length = *buffer_size;
    size_t buf_size = sizeof(buf);
    status = libredxx_write(device, buf, &buf_size);
    if (status != LIBREDXX_STATUS_SUCCESS)
    {
        return status;
    }
    // TODO clear read messages
    return libredxx_read(device, buffer, buffer_size);
}
