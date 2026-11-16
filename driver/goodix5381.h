/*
 * Goodix 27c6:5381 fingerprint sensor driver for libfprint
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#pragma once

#include "fpi-image-device.h"

#define GOODIX5381_VID             0x27c6
#define GOODIX5381_PID             0x5381

#define GOODIX5381_IFACE_CTRL      0
#define GOODIX5381_IFACE_DATA      1

#define GOODIX5381_EP_OUT          0x03
#define GOODIX5381_EP_IN           0x81

#define GOODIX5381_MAX_PACKET_SIZE 64
#define GOODIX5381_TIMEOUT_MS      1000

/* USB CDC-ACM Control Requests */
#define CDC_REQ_SET_LINE_CODING        0x20
#define CDC_REQ_SET_CONTROL_LINE_STATE 0x22
#define CDC_CONTROL_LINE_DTR_RTS       0x0003

G_BEGIN_DECLS

#define FPI_TYPE_DEVICE_GOODIX5381 (fpi_device_goodix5381_get_type ())
G_DECLARE_FINAL_TYPE (FpiDeviceGoodix5381, fpi_device_goodix5381, FPI, DEVICE_GOODIX5381, FpImageDevice)

G_END_DECLS
