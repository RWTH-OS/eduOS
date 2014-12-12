/*
 * Copyright (c) 2014, Stefan Lankes, Daniel Krebs, RWTH Aachen University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <eduos/stdio.h>
#include <eduos/string.h>
#include <eduos/mailbox.h>
#include <asm/io.h>
#include <asm/uart.h>
#include <asm/irq.h>
#include <asm/irqflags.h>
#ifdef CONFIG_PCI
#include <asm/pci.h>
#endif

#ifdef CONFIG_UART

/*
 * This implementation based on following tutorial:
 * http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming
 */

#define UART_RX			0	/* In:  Receive buffer */
#define UART_TX			0	/* Out: Transmit buffer */
#define UART_IER		1	/* Out: Interrupt Enable Register */
#define UART_FCR		2	/* Out: FIFO Control Register */
#define UART_IIR        2   /* In:  Interrupt ID Register */

#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */

#define UART_IIR_NO_INT		0x01 /* No interrupts pending */
#define UART_IIR_ID			0x06 /* Mask for the interrupt ID */
#define UART_IIR_MSI		0x00 /* Modem status interrupt */
#define UART_IIR_THRI		0x02 /* Transmitter holding register empty */
#define UART_IIR_RDI		0x04 /* Receiver data interrupt */
#define UART_IIR_RLSI		0x06 /* Receiver line status interrupt */

#define UART_FCR_ENABLE_FIFO	0x01 /* Enable the FIFO */
#define UART_FCR_CLEAR_RCVR		0x02 /* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT		0x04 /* Clear the XMIT FIFO */
#define UART_FCR_TRIGGER_MASK	0xC0 /* Mask for the FIFO trigger range */
#define UART_FCR_TRIGGER_1		0x00 /* Mask for trigger set at 1 */

#define UART_DLL			0 /* Out: Divisor Latch Low */
#define UART_DLM			1 /* Out: Divisor Latch High */
#define UART_LCR			3 /* Out: Line Control Register */

#define UART_LCR_DLAB		0x80 /* Divisor latch access bit */
#define UART_LCR_SBC		0x40 /* Set break control */
#define UART_LCR_SPAR		0x20 /* Stick parity (?) */
#define UART_LCR_EPAR		0x10 /* Even parity select */
#define UART_LCR_PARITY		0x08 /* Parity Enable */
#define UART_LCR_STOP		0x04 /* Stop bits: 0=1 bit, 1=2 bits */
#define UART_LCR_WLEN8		0x03 /* Wordlength: 8 bits */

static uint32_t	iobase = 0;
static tid_t	id;
static mailbox_uint8_t input_queue;

/* Get a single character on a serial device */
static unsigned char uart_getchar(void)
{
	return inportb(iobase + UART_RX);
}

/* Puts a single character on a serial device */
int uart_putchar(unsigned char c)
{
	if (!iobase)
		return 0;

	outportb(iobase + UART_TX, c);

	return (int) c;
}

/* Uses the routine above to output a string... */
int uart_puts(const char *text)
{
	size_t i, len = strlen(text);

	if (!iobase)
		return 0;

	for (i = 0; i < len; i++)
		uart_putchar(text[i]);

	return len;
}

/* Handles all UART's interrupt */
static void uart_handler(struct state *s)
{
	unsigned char c = inportb(iobase + UART_IIR);

	while (!(c & UART_IIR_NO_INT)) {
		if (c & UART_IIR_RDI) {
			c = uart_getchar();

			mailbox_uint8_post(&input_queue, c);
		}

		c = inportb(iobase + UART_IIR);
	}
}

/* thread entry point => enable all incoming messages */
static int uart_thread(void* arg)
{
	unsigned char c = 0;

	while(1) {
		mailbox_uint8_fetch(&input_queue, &c);

		kputchar(c);
	}

	return 0;
}

int uart_enable_input(void)
{
	int err = create_kernel_task(&id, uart_thread, NULL, HIGH_PRIO);

	if (BUILTIN_EXPECT(err, 0))
		kprintf("Failed to create task (uart): %d\n", err);
	else
		kputs("Create task to handle incoming messages (uart)\n");

	return err;
}

static void uart_config(void)
{
	mailbox_uint8_init(&input_queue);

	/*
	 * enable FIFOs
	 * clear RX and TX FIFO
	 * set irq trigger to 8 bytes
	 */
	outportb(iobase + UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT | UART_FCR_TRIGGER_1);

	/*
	 * 8bit word length
	 * 1 stop bit
	 * no partity
	 * set DLAB=1
	 */
	char lcr = UART_LCR_WLEN8 | UART_LCR_DLAB;
	outportb(iobase + UART_LCR, lcr);

	/*
	 * set baudrate to 115200 (on qemu)
	 */
	outportb(iobase + UART_DLL, 0x01);
	outportb(iobase + UART_DLM, 0x00);

	/* set DLAB=0 */
	outportb(iobase + UART_LCR, lcr & (~UART_LCR_DLAB));

	/* enable interrupt */
	outportb(iobase + UART_IER, UART_IER_RDI | UART_IER_RLSI | UART_IER_THRI);
}

int uart_init(void)
{
#ifdef CONFIG_PCI
	pci_info_t pci_info;

	// Searching for Intel's UART device
	if (pci_get_device_info(0x8086, 0x0936, &pci_info) == 0)
		goto Lsuccess;
 	// Searching for Qemu's UART device
	if (pci_get_device_info(0x1b36, 0x0002, &pci_info) == 0)
		goto Lsuccess;

	return -1;

Lsuccess:
	// we use COM1
	iobase = 0x03f8;
	irq_install_handler(32+4, uart_handler);

	// configure uart
	uart_config();
#endif

	return 0;
}

#endif
