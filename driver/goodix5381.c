/*
 * Goodix 27c6:5381 fingerprint sensor driver for libfprint
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define FP_COMPONENT "goodix5381"

#include "drivers_api.h"
#include "goodix5381.h"

struct _FpiDeviceGoodix5381
{
  FpImageDevice parent;
  FpiSsm       *task_ssm;
};

G_DEFINE_TYPE (FpiDeviceGoodix5381, fpi_device_goodix5381, FP_TYPE_IMAGE_DEVICE)

/* Frame 38: Initial host probe (command 0xa7) */
static const guint8 cmd_probe_38[GOODIX5381_MAX_PACKET_SIZE] = {
  0x00, 0x03, 0x00, 0x00, 0x00, 0xa7,
};

/* Frame 42: Initial register/interrupt mask configuration */
static const guint8 cmd_init_cfg_42[GOODIX5381_MAX_PACKET_SIZE] = {
  0xa0, 0x03, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xf7,
  0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

enum {
  ACTIVATE_SET_LINE_CODING,
  ACTIVATE_SET_CONTROL_LINE,
  ACTIVATE_SEND_PROBE,
  ACTIVATE_RECV_PROBE,
  ACTIVATE_SEND_INIT_CFG,
  ACTIVATE_RECV_INIT_CFG,
  /* TODO: Add remaining initialization frames (48+) and finger capture states */
  ACTIVATE_NUM_STATES,
};

static void
ssm_transfer_cb (FpiUsbTransfer *transfer,
                 FpDevice       *dev,
                 gpointer        user_data,
                 GError         *error)
{
  FpiSsm *ssm = user_data;

  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }

  fpi_ssm_next_state (ssm);
}

static void
send_bulk_packet (FpiSsm       *ssm,
                  FpDevice     *dev,
                  const guint8 *data,
                  gsize         len)
{
  FpiUsbTransfer *transfer = fpi_usb_transfer_new (dev);

  transfer->ssm = ssm;
  fpi_usb_transfer_fill_bulk (transfer, GOODIX5381_EP_OUT, len);
  memcpy (transfer->buffer, data, len);
  fpi_usb_transfer_submit (transfer, GOODIX5381_TIMEOUT_MS, NULL, ssm_transfer_cb, ssm);
}

static void
recv_bulk_packet (FpiSsm *ssm, FpDevice *dev)
{
  FpiUsbTransfer *transfer = fpi_usb_transfer_new (dev);

  transfer->ssm = ssm;
  fpi_usb_transfer_fill_bulk (transfer, GOODIX5381_EP_IN, GOODIX5381_MAX_PACKET_SIZE);
  fpi_usb_transfer_submit (transfer, GOODIX5381_TIMEOUT_MS, NULL, ssm_transfer_cb, ssm);
}

static void
activate_run_state (FpiSsm *ssm, FpDevice *dev)
{
  switch (fpi_ssm_get_cur_state (ssm))
    {
    case ACTIVATE_SET_LINE_CODING:
      {
        /* 115200 baud, 1 stop bit, no parity, 8 data bits */
        static const guint8 line_coding[7] = { 0x00, 0xc2, 0x01, 0x00, 0x00, 0x00, 0x08 };
        FpiUsbTransfer *transfer = fpi_usb_transfer_new (dev);

        transfer->ssm = ssm;
        fpi_usb_transfer_fill_control (transfer,
                                       G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
                                       G_USB_DEVICE_REQUEST_TYPE_CLASS,
                                       G_USB_DEVICE_RECIPIENT_INTERFACE,
                                       CDC_REQ_SET_LINE_CODING,
                                       0x0000,
                                       GOODIX5381_IFACE_CTRL,
                                       sizeof (line_coding));
        memcpy (transfer->buffer, line_coding, sizeof (line_coding));
        fpi_usb_transfer_submit (transfer, GOODIX5381_TIMEOUT_MS, NULL, ssm_transfer_cb, ssm);
        break;
      }

    case ACTIVATE_SET_CONTROL_LINE:
      {
        FpiUsbTransfer *transfer = fpi_usb_transfer_new (dev);

        transfer->ssm = ssm;
        fpi_usb_transfer_fill_control (transfer,
                                       G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
                                       G_USB_DEVICE_REQUEST_TYPE_CLASS,
                                       G_USB_DEVICE_RECIPIENT_INTERFACE,
                                       CDC_REQ_SET_CONTROL_LINE_STATE,
                                       CDC_CONTROL_LINE_DTR_RTS,
                                       GOODIX5381_IFACE_CTRL,
                                       0);
        fpi_usb_transfer_submit (transfer, GOODIX5381_TIMEOUT_MS, NULL, ssm_transfer_cb, ssm);
        break;
      }

    case ACTIVATE_SEND_PROBE:
      send_bulk_packet (ssm, dev, cmd_probe_38, sizeof (cmd_probe_38));
      break;

    case ACTIVATE_RECV_PROBE:
      recv_bulk_packet (ssm, dev);
      break;

    case ACTIVATE_SEND_INIT_CFG:
      send_bulk_packet (ssm, dev, cmd_init_cfg_42, sizeof (cmd_init_cfg_42));
      break;

    case ACTIVATE_RECV_INIT_CFG:
      recv_bulk_packet (ssm, dev);
      break;
    }
}

static void
activate_complete (FpiSsm *ssm, FpDevice *dev, GError *error)
{
  FpiDeviceGoodix5381 *self = FPI_DEVICE_GOODIX5381 (dev);
  FpImageDevice *img_dev = FP_IMAGE_DEVICE (dev);

  self->task_ssm = NULL;

  if (!error)
    fp_dbg ("Goodix 5381 initial handshake completed (remaining frames TODO)");

  fpi_image_device_activate_complete (img_dev, error);
}

static void
dev_open (FpImageDevice *dev)
{
  GUsbDevice *usb_dev = fpi_device_get_usb_device (FP_DEVICE (dev));
  GError *error = NULL;

  if (!g_usb_device_claim_interface (usb_dev, GOODIX5381_IFACE_CTRL, 0, &error))
    {
      fpi_image_device_open_complete (dev, error);
      return;
    }

  if (!g_usb_device_claim_interface (usb_dev, GOODIX5381_IFACE_DATA, 0, &error))
    {
      g_usb_device_release_interface (usb_dev, GOODIX5381_IFACE_CTRL, 0, NULL);
      fpi_image_device_open_complete (dev, error);
      return;
    }

  fpi_image_device_open_complete (dev, NULL);
}

static void
dev_close (FpImageDevice *dev)
{
  GUsbDevice *usb_dev = fpi_device_get_usb_device (FP_DEVICE (dev));
  GError *error = NULL;

  g_usb_device_release_interface (usb_dev, GOODIX5381_IFACE_DATA, 0, NULL);
  g_usb_device_release_interface (usb_dev, GOODIX5381_IFACE_CTRL, 0, &error);

  fpi_image_device_close_complete (dev, error);
}

static void
dev_activate (FpImageDevice *dev)
{
  FpiDeviceGoodix5381 *self = FPI_DEVICE_GOODIX5381 (dev);

  self->task_ssm = fpi_ssm_new (FP_DEVICE (dev), activate_run_state, ACTIVATE_NUM_STATES);
  fpi_ssm_start (self->task_ssm, activate_complete);
}

static void
dev_deactivate (FpImageDevice *dev)
{
  fpi_image_device_deactivate_complete (dev, NULL);
}

static const FpIdEntry id_table[] = {
  { .vid = GOODIX5381_VID, .pid = GOODIX5381_PID, .driver_data = 0 },
  { .vid = 0, .pid = 0, .driver_data = 0 },
};

static void
fpi_device_goodix5381_init (FpiDeviceGoodix5381 *self)
{
}

static void
fpi_device_goodix5381_class_init (FpiDeviceGoodix5381Class *klass)
{
  FpDeviceClass *dev_class = FP_DEVICE_CLASS (klass);
  FpImageDeviceClass *img_class = FP_IMAGE_DEVICE_CLASS (klass);

  dev_class->id = FP_COMPONENT;
  dev_class->full_name = "Goodix 27c6:5381 Fingerprint Sensor";
  dev_class->type = FP_DEVICE_TYPE_USB;
  dev_class->scan_type = FP_SCAN_TYPE_PRESS;
  dev_class->id_table = id_table;

  /* TODO: Update once exact sensor matrix dimensions are extracted from capture */
  img_class->img_width = 96;
  img_class->img_height = 96;

  img_class->img_open = dev_open;
  img_class->img_close = dev_close;
  img_class->activate = dev_activate;
  img_class->deactivate = dev_deactivate;
}
