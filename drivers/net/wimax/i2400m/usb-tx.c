/*
 * Intel Wireless WiMAX Connection 2400m
 * USB specific TX handling
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 *  - Initial implementation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Split transport/device specific
 *
 *
 * Takes the TX messages in the i2400m's driver TX FIFO and sends them
 * to the device until there are no more.
 *
 * If we fail sending the message, we just drop it. There isn't much
 * we can do at this point. We could also retry, but the USB stack has
 * already retried and still failed, so there is not much of a
 * point. As well, most of the traffic is network, which has recovery
 * methods for dropped packets.
 *
 * For sending we just obtain a FIFO buffer to send, send it to the
 * USB bulk out, tell the TX FIFO code we have sent it; query for
 * another one, etc... until done.
 *
 * We use a thread so we can call usb_autopm_enable() and
 * usb_autopm_disable() for each transaction; this way when the device
 * goes idle, it will suspend. It also has less overhead than a
 * dedicated workqueue, as it is being used for a single task.
 *
 * ROADMAP
 *
 * i2400mu_tx_setup()
 * i2400mu_tx_release()
 *
 * i2400mu_bus_tx_kick()	- Called by the tx.c code when there
 *                                is new data in the FIFO.
 * i2400mu_txd()
 *   i2400m_tx_msg_get()
 *   i2400m_tx_msg_sent()
 */
#include "i2400m-usb.h"


#define D_SUBMODULE tx
#include "usb-debug-levels.h"


/*
 * Get the next TX message in the TX FIFO and send it to the device
 *
 * Note that any iteration consumes a message to be sent, no matter if
 * it succeeds or fails (we have no real way to retry or complain).
 *
 * Return: 0 if ok, < 0 errno code on hard error.
 */
static
int i2400mu_tx(struct i2400mu *i2400mu, struct i2400m_msg_hdr *tx_msg,
	       size_t tx_msg_size)
{
	int result = 0;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;
	int usb_pipe, sent_size, do_autopm;
	struct usb_endpoint_descriptor *epd;

	d_fnstart(4, dev, "(i2400mu %p)\n", i2400mu);
	do_autopm = atomic_read(&i2400mu->do_autopm);
	result = do_autopm ?
		usb_autopm_get_interface(i2400mu->usb_iface) : 0;
	if (result < 0) {
		dev_err(dev, "TX: can't get autopm: %d\n", result);
		do_autopm = 0;
	}
	epd = usb_get_epd(i2400mu->usb_iface, I2400MU_EP_BULK_OUT);
	usb_pipe = usb_sndbulkpipe(i2400mu->usb_dev, epd->bEndpointAddress);
retry:
	result = usb_bulk_msg(i2400mu->usb_dev, usb_pipe,
			      tx_msg, tx_msg_size, &sent_size, HZ);
	usb_mark_last_busy(i2400mu->usb_dev);
	switch (result) {
	case 0:
		if (sent_size != tx_msg_size) {	/* Too short? drop it */
			dev_err(dev, "TX: short write (%d B vs %zu "
				"expected)\n", sent_size, tx_msg_size);
			result = -EIO;
		}
		break;
	case -EINVAL:			/* while removing driver */
	case -ENODEV:			/* dev disconnect ... */
	case -ENOENT:			/* just ignore it */
	case -ESHUTDOWN:		/* and exit */
	case -ECONNRESET:
		result = -ESHUTDOWN;
		break;
	default:			/* Some error? */
		if (edc_inc(&i2400mu->urb_edc,
			    EDC_MAX_ERRORS, EDC_ERROR_TIMEFRAME)) {
			dev_err(dev, "TX: maximum errors in URB "
				"exceeded; resetting device\n");
			usb_queue_reset_device(i2400mu->usb_iface);
		} else {
			dev_err(dev, "TX: cannot send URB; retrying. "
				"tx_msg @%zu %zu B [%d sent]: %d\n",
				(void *) tx_msg - i2400m->tx_buf,
				tx_msg_size, sent_size, result);
			goto retry;
		}
	}
	if (do_autopm)
		usb_autopm_put_interface(i2400mu->usb_iface);
	d_fnend(4, dev, "(i2400mu %p) = result\n", i2400mu);
	return result;
}


/*
 * Get the next TX message in the TX FIFO and send it to the device
 *
 * Note we exit the loop if i2400mu_tx() fails; that funtion only
 * fails on hard error (failing to tx a buffer not being one of them,
 * see its doc).
 *
 * Return: 0
 */
static
int i2400mu_txd(void *_i2400mu)
{
	int result = 0;
	struct i2400mu *i2400mu = _i2400mu;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;
	struct i2400m_msg_hdr *tx_msg;
	size_t tx_msg_size;

	d_fnstart(4, dev, "(i2400mu %p)\n", i2400mu);

	while (1) {
		d_printf(2, dev, "TX: waiting for messages\n");
		tx_msg = NULL;
		wait_event_interruptible(
			i2400mu->tx_wq,
			(kthread_should_stop()	/* check this first! */
			 || (tx_msg = i2400m_tx_msg_get(i2400m, &tx_msg_size)))
			);
		if (kthread_should_stop())
			break;
		WARN_ON(tx_msg == NULL);	/* should not happen...*/
		d_printf(2, dev, "TX: submitting %zu bytes\n", tx_msg_size);
		d_dump(5, dev, tx_msg, tx_msg_size);
		/* Yeah, we ignore errors ... not much we can do */
		i2400mu_tx(i2400mu, tx_msg, tx_msg_size);
		i2400m_tx_msg_sent(i2400m);	/* ack it, advance the FIFO */
		if (result < 0)
			break;
	}
	d_fnend(4, dev, "(i2400mu %p) = %d\n", i2400mu, result);
	return result;
}


/*
 * i2400m TX engine notifies us that there is data in the FIFO ready
 * for TX
 *
 * If there is a URB in flight, don't do anything; when it finishes,
 * it will see there is data in the FIFO and send it. Else, just
 * submit a write.
 */
void i2400mu_bus_tx_kick(struct i2400m *i2400m)
{
	struct i2400mu *i2400mu = container_of(i2400m, struct i2400mu, i2400m);
	struct device *dev = &i2400mu->usb_iface->dev;

	d_fnstart(3, dev, "(i2400m %p) = void\n", i2400m);
	wake_up_all(&i2400mu->tx_wq);
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}


int i2400mu_tx_setup(struct i2400mu *i2400mu)
{
	int result = 0;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;

	i2400mu->tx_kthread = kthread_run(i2400mu_txd, i2400mu, "%s-tx",
					  wimax_dev->name);
	if (IS_ERR(i2400mu->tx_kthread)) {
		result = PTR_ERR(i2400mu->tx_kthread);
		dev_err(dev, "TX: cannot start thread: %d\n", result);
	}
	return result;
}

void i2400mu_tx_release(struct i2400mu *i2400mu)
{
	kthread_stop(i2400mu->tx_kthread);
}
