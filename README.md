# libredxx

[libre](https://en.wikipedia.org/wiki/Alternative_terms_for_free_software#FLOSS)dxx
is a library for communicating with FTDI devices.

## Features

- lightweight: no hidden threads or dependencies
- cross-platform: supports Windows, Linux, and macOS
- full-featured: supports D2XX and D3XX devices
- open-source: auditable code that builds anywhere

## Documentation

Examples can be found under the [example](example) folder, to build these with
CMake add `-D LIBREDXX_ENABLE_EXAMPLES=ON`.

API documentation can be found within [libredxx.h](libredxx/libredxx.h).

### FT260 Notes

In the case of the FT260, libredxx_write and librexx_read will write what is supplied directly to the device. However, 
device communication occurs through specially formatted HID reports, which are used in most all use cases.

It is recommended to use the API in libredxx_ft260.h instead of calling libredxx_read and libredxx_write directly for
this device, unless passing already formatted HID reports to these functions.

## License

libredxx is licensed under the MIT license, a copy can be found in
[LICENSE](LICENSE).

