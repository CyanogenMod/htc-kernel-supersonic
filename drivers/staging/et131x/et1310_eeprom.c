/*
 * Agere Systems Inc.
 * 10/100/1000 Base-T Ethernet Driver for the ET1301 and ET131x series MACs
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *   http://www.agere.com
 *
 *------------------------------------------------------------------------------
 *
 * et1310_eeprom.c - Code used to access the device's EEPROM
 *
 *------------------------------------------------------------------------------
 *
 * SOFTWARE LICENSE
 *
 * This software is provided subject to the following terms and conditions,
 * which you should read carefully before using the software.  Using this
 * software indicates your acceptance of these terms and conditions.  If you do
 * not agree with these terms and conditions, do not use the software.
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source or binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 * . Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following Disclaimer as comments in the code as
 *    well as in the documentation and/or other materials provided with the
 *    distribution.
 *
 * . Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following Disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * . Neither the name of Agere Systems Inc. nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Disclaimer
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
 * USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
 * RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include "et131x_version.h"
#include "et131x_defs.h"

#include <linux/pci.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>

#include "et1310_phy.h"
#include "et1310_pm.h"
#include "et1310_jagcore.h"
#include "et1310_eeprom.h"

#include "et131x_adapter.h"
#include "et131x_initpci.h"
#include "et131x_isr.h"

#include "et1310_tx.h"


/*
 * EEPROM Defines
 */

/* LBCIF Register Groups (addressed via 32-bit offsets) */
#define LBCIF_DWORD0_GROUP_OFFSET       0xAC
#define LBCIF_DWORD1_GROUP_OFFSET       0xB0

/* LBCIF Registers (addressed via 8-bit offsets) */
#define LBCIF_ADDRESS_REGISTER_OFFSET   0xAC
#define LBCIF_DATA_REGISTER_OFFSET      0xB0
#define LBCIF_CONTROL_REGISTER_OFFSET   0xB1
#define LBCIF_STATUS_REGISTER_OFFSET    0xB2

/* LBCIF Control Register Bits */
#define LBCIF_CONTROL_SEQUENTIAL_READ   0x01
#define LBCIF_CONTROL_PAGE_WRITE        0x02
#define LBCIF_CONTROL_UNUSED1           0x04
#define LBCIF_CONTROL_EEPROM_RELOAD     0x08
#define LBCIF_CONTROL_UNUSED2           0x10
#define LBCIF_CONTROL_TWO_BYTE_ADDR     0x20
#define LBCIF_CONTROL_I2C_WRITE         0x40
#define LBCIF_CONTROL_LBCIF_ENABLE      0x80

/* LBCIF Status Register Bits */
#define LBCIF_STATUS_PHY_QUEUE_AVAIL    0x01
#define LBCIF_STATUS_I2C_IDLE           0x02
#define LBCIF_STATUS_ACK_ERROR          0x04
#define LBCIF_STATUS_GENERAL_ERROR      0x08
#define LBCIF_STATUS_UNUSED             0x30
#define LBCIF_STATUS_CHECKSUM_ERROR     0x40
#define LBCIF_STATUS_EEPROM_PRESENT     0x80

/* Miscellaneous Constraints */
#define MAX_NUM_REGISTER_POLLS          1000
#define MAX_NUM_WRITE_RETRIES           2

/*
 * Define macros that allow individual register values to be extracted from a
 * DWORD1 register grouping
 */
#define EXTRACT_DATA_REGISTER(x)    (u8)(x & 0xFF)
#define EXTRACT_STATUS_REGISTER(x)  (u8)((x >> 16) & 0xFF)
#define EXTRACT_CONTROL_REG(x)      (u8)((x >> 8) & 0xFF)

/**
 * EepromWriteByte - Write a byte to the ET1310's EEPROM
 * @etdev: pointer to our private adapter structure
 * @addr: the address to write
 * @data: the value to write
 *
 * Returns SUCCESS or FAILURE
 */
int EepromWriteByte(struct et131x_adapter *etdev, u32 addr, u8 data)
{
	struct pci_dev *pdev = etdev->pdev;
	int index;
	int retries;
	int err = 0;
	int i2c_wack = 0;
	int writeok = 0;
	u8 control;
	u8 status = 0;
	u32 dword1 = 0;
	u32 val = 0;

	/*
	 * The following excerpt is from "Serial EEPROM HW Design
	 * Specification" Version 0.92 (9/20/2004):
	 *
	 * Single Byte Writes
	 *
	 * For an EEPROM, an I2C single byte write is defined as a START
	 * condition followed by the device address, EEPROM address, one byte
	 * of data and a STOP condition.  The STOP condition will trigger the
	 * EEPROM's internally timed write cycle to the nonvolatile memory.
	 * All inputs are disabled during this write cycle and the EEPROM will
	 * not respond to any access until the internal write is complete.
	 * The steps to execute a single byte write are as follows:
	 *
	 * 1. Check LBCIF Status Register for bits 6 & 3:2 all equal to 0 and
	 *    bits 7,1:0 both equal to 1, at least once after reset.
	 *    Subsequent operations need only to check that bits 1:0 are
	 *    equal to 1 prior to starting a single byte write.
	 *
	 * 2. Write to the LBCIF Control Register:  bit 7=1, bit 6=1, bit 3=0,
	 *    and bits 1:0 both =0.  Bit 5 should be set according to the
	 *    type of EEPROM being accessed (1=two byte addressing, 0=one
	 *    byte addressing).
	 *
	 * 3. Write the address to the LBCIF Address Register.
	 *
	 * 4. Write the data to the LBCIF Data Register (the I2C write will
	 *    begin).
	 *
	 * 5. Monitor bit 1:0 of the LBCIF Status Register.  When bits 1:0 are
	 *    both equal to 1, the I2C write has completed and the internal
	 *    write cycle of the EEPROM is about to start. (bits 1:0 = 01 is
	 *    a legal state while waiting from both equal to 1, but bits
	 *    1:0 = 10 is invalid and implies that something is broken).
	 *
	 * 6. Check bit 3 of the LBCIF Status Register.  If  equal to 1, an
	 *    error has occurred.
	 *
	 * 7. Check bit 2 of the LBCIF Status Register.  If equal to 1 an ACK
	 *    error has occurred on the address phase of the write.  This
	 *    could be due to an actual hardware failure or the EEPROM may
	 *    still be in its internal write cycle from a previous write.
	 *    This write operation was ignored and must be repeated later.
	 *
	 * 8. Set bit 6 of the LBCIF Control Register = 0. If another write is
	 *    required, go to step 1.
	 */

	/* Step 1: */
	for (index = 0; index < MAX_NUM_REGISTER_POLLS; index++) {
		/* Read registers grouped in DWORD1 */
		if (pci_read_config_dword(pdev, LBCIF_DWORD1_GROUP_OFFSET,
					  &dword1)) {
			err = 1;
			break;
		}

		status = EXTRACT_STATUS_REGISTER(dword1);

		if (status & LBCIF_STATUS_PHY_QUEUE_AVAIL &&
			status & LBCIF_STATUS_I2C_IDLE)
			/* bits 1:0 are equal to 1 */
			break;
	}

	if (err || (index >= MAX_NUM_REGISTER_POLLS))
		return FAILURE;

	/* Step 2: */
	control = 0;
	control |= LBCIF_CONTROL_LBCIF_ENABLE | LBCIF_CONTROL_I2C_WRITE;

	if (pci_write_config_byte(pdev, LBCIF_CONTROL_REGISTER_OFFSET,
				  control)) {
		return FAILURE;
	}

	i2c_wack = 1;

	/* Prepare EEPROM address for Step 3 */

	for (retries = 0; retries < MAX_NUM_WRITE_RETRIES; retries++) {
		/* Step 3:*/
		if (pci_write_config_dword(pdev, LBCIF_ADDRESS_REGISTER_OFFSET,
					   addr)) {
			break;
		}

		/* Step 4: */
		if (pci_write_config_byte(pdev, LBCIF_DATA_REGISTER_OFFSET,
					  data)) {
			break;
		}

		/* Step 5: */
		for (index = 0; index < MAX_NUM_REGISTER_POLLS; index++) {
			/* Read registers grouped in DWORD1 */
			if (pci_read_config_dword(pdev,
						  LBCIF_DWORD1_GROUP_OFFSET,
						  &dword1)) {
				err = 1;
				break;
			}

			status = EXTRACT_STATUS_REGISTER(dword1);

			if (status & LBCIF_STATUS_PHY_QUEUE_AVAIL &&
				status & LBCIF_STATUS_I2C_IDLE) {
				/* I2C write complete */
				break;
			}
		}

		if (err || (index >= MAX_NUM_REGISTER_POLLS))
			break;

		/*
		 * Step 6: Don't break here if we are revision 1, this is
		 *	   so we do a blind write for load bug.
		 */
		if (status & LBCIF_STATUS_GENERAL_ERROR
		    && etdev->pdev->revision == 0) {
			break;
		}

		/* Step 7 */
		if (status & LBCIF_STATUS_ACK_ERROR) {
			/*
			 * This could be due to an actual hardware failure
			 * or the EEPROM may still be in its internal write
			 * cycle from a previous write. This write operation
			 * was ignored and must be repeated later.
			 */
			udelay(10);
			continue;
		}

		writeok = 1;
		break;
	}

	/* Step 8: */
	udelay(10);
	index = 0;
	while (i2c_wack) {
		control &= ~LBCIF_CONTROL_I2C_WRITE;

		if (pci_write_config_byte(pdev, LBCIF_CONTROL_REGISTER_OFFSET,
					  control)) {
			writeok = 0;
		}

		/* Do read until internal ACK_ERROR goes away meaning write
		 * completed
		 */
		do {
			pci_write_config_dword(pdev,
					       LBCIF_ADDRESS_REGISTER_OFFSET,
					       addr);
			do {
				pci_read_config_dword(pdev,
					LBCIF_DATA_REGISTER_OFFSET, &val);
			} while ((val & 0x00010000) == 0);
		} while (val & 0x00040000);

		control = EXTRACT_CONTROL_REG(val);

		if (control != 0xC0 || index == 10000)
			break;

		index++;
	}

	return writeok ? SUCCESS : FAILURE;
}

/**
 * EepromReadByte - Read a byte from the ET1310's EEPROM
 * @etdev: pointer to our private adapter structure
 * @addr: the address from which to read
 * @pdata: a pointer to a byte in which to store the value of the read
 * @eeprom_id: the ID of the EEPROM
 * @addrmode: how the EEPROM is to be accessed
 *
 * Returns SUCCESS or FAILURE
 */
int EepromReadByte(struct et131x_adapter *etdev, u32 addr, u8 *pdata)
{
	struct pci_dev *pdev = etdev->pdev;
	int index;
	int err = 0;
	u8 control;
	u8 status = 0;
	u32 dword1 = 0;

	/*
	 * The following excerpt is from "Serial EEPROM HW Design
	 * Specification" Version 0.92 (9/20/2004):
	 *
	 * Single Byte Reads
	 *
	 * A single byte read is similar to the single byte write, with the
	 * exception of the data flow:
	 *
	 * 1. Check LBCIF Status Register for bits 6 & 3:2 all equal to 0 and
	 *    bits 7,1:0 both equal to 1, at least once after reset.
	 *    Subsequent operations need only to check that bits 1:0 are equal
	 *    to 1 prior to starting a single byte read.
	 *
	 * 2. Write to the LBCIF Control Register:  bit 7=1, bit 6=0, bit 3=0,
	 *    and bits 1:0 both =0.  Bit 5 should be set according to the type
	 *    of EEPROM being accessed (1=two byte addressing, 0=one byte
	 *    addressing).
	 *
	 * 3. Write the address to the LBCIF Address Register (I2C read will
	 *    begin).
	 *
	 * 4. Monitor bit 0 of the LBCIF Status Register.  When =1, I2C read
	 *    is complete. (if bit 1 =1 and bit 0 stays =0, a hardware failure
	 *    has occurred).
	 *
	 * 5. Check bit 2 of the LBCIF Status Register.  If =1, then an error
	 *    has occurred.  The data that has been returned from the PHY may
	 *    be invalid.
	 *
	 * 6. Regardless of error status, read data byte from LBCIF Data
	 *    Register.  If another byte is required, go to step 1.
	 */

	/* Step 1: */
	for (index = 0; index < MAX_NUM_REGISTER_POLLS; index++) {
		/* Read registers grouped in DWORD1 */
		if (pci_read_config_dword(pdev, LBCIF_DWORD1_GROUP_OFFSET,
					  &dword1)) {
			err = 1;
			break;
		}

		status = EXTRACT_STATUS_REGISTER(dword1);

		if (status & LBCIF_STATUS_PHY_QUEUE_AVAIL &&
		    status & LBCIF_STATUS_I2C_IDLE) {
			/* bits 1:0 are equal to 1 */
			break;
		}
	}

	if (err || (index >= MAX_NUM_REGISTER_POLLS))
		return FAILURE;

	/* Step 2: */
	control = 0;
	control |= LBCIF_CONTROL_LBCIF_ENABLE;

	if (pci_write_config_byte(pdev, LBCIF_CONTROL_REGISTER_OFFSET,
				  control)) {
		return FAILURE;
	}

	/* Step 3: */

	if (pci_write_config_dword(pdev, LBCIF_ADDRESS_REGISTER_OFFSET,
				   addr)) {
		return FAILURE;
	}

	/* Step 4: */
	for (index = 0; index < MAX_NUM_REGISTER_POLLS; index++) {
		/* Read registers grouped in DWORD1 */
		if (pci_read_config_dword(pdev, LBCIF_DWORD1_GROUP_OFFSET,
					  &dword1)) {
			err = 1;
			break;
		}

		status = EXTRACT_STATUS_REGISTER(dword1);

		if (status & LBCIF_STATUS_PHY_QUEUE_AVAIL
		    && status & LBCIF_STATUS_I2C_IDLE) {
			/* I2C read complete */
			break;
		}
	}

	if (err || (index >= MAX_NUM_REGISTER_POLLS))
		return FAILURE;

	/* Step 6: */
	*pdata = EXTRACT_DATA_REGISTER(dword1);

	return (status & LBCIF_STATUS_ACK_ERROR) ? FAILURE : SUCCESS;
}
