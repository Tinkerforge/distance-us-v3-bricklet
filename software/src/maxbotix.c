/* distance-us-v2-bricklet
 * Copyright (C) 2019 Olaf Lüke <olaf@tinkerforge.com>
 *
 * maxbotix.c: Driver for MaxBotix Distance US sensor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "maxbotix.h"

#include "configs/config_maxbotix.h"

#include "xmc_uart.h"
#include "xmc_gpio.h"

#define maxbotix_rx_irq_handler  IRQ_Hdlr_11

Maxbotix maxbotix;

void maxbotix_parse(const char data) {
	switch(maxbotix.state) {
		case 0: {
			maxbotix.value = 0;

			if(data == 'R') {
				maxbotix.state = 1;
			}

			break;
		}

		case 1: {
			if(data >= '0' && data <= '9') {
				maxbotix.value = maxbotix.value*10 + (data - '0');
			} else {
				if(data == '\r') {
					maxbotix.distance = maxbotix.value;
				}
				maxbotix.state = 0;
			}

			break;
		}
	}
}

void __attribute__((optimize("-O3"))) __attribute__ ((section (".ram_code"))) maxbotix_rx_irq_handler(void) {
	while(!XMC_USIC_CH_RXFIFO_IsEmpty(MAXBOTIX_USIC)) {
		const char data = (char)MAXBOTIX_USIC->OUTR;
		maxbotix_parse(data);
	}
}


void maxbotix_tick(void) {
	// TODO: Distance LED
}

void maxbotix_init_uart(void) {
	// RX pin configuration
	const XMC_GPIO_CONFIG_t rx_pin_config = {
		.mode             = XMC_GPIO_MODE_INPUT_PULL_UP,
		.input_hysteresis = XMC_GPIO_INPUT_HYSTERESIS_STANDARD
	};

	// Configure  pins
	XMC_GPIO_Init(MAXBOTIX_RX_PIN, &rx_pin_config);

	// Initialize USIC channel in UART master mode
	// USIC channel configuration
	XMC_UART_CH_CONFIG_t config;
	config.oversampling = 16;
	config.frame_length = 8;
	config.baudrate     = 9600;
	config.stop_bits    = 1;
	config.data_bits    = 8;
	config.parity_mode  = XMC_USIC_CH_PARITY_MODE_NONE;
	XMC_UART_CH_Init(MAXBOTIX_USIC, &config);

	// Set input source path
	XMC_UART_CH_SetInputSource(MAXBOTIX_USIC, MAXBOTIX_RX_INPUT, MAXBOTIX_RX_SOURCE);
	XMC_USIC_CH_EnableInputInversion(MAXBOTIX_USIC, XMC_USIC_CH_INPUT_DX0);

	// Configure receive FIFO
	XMC_USIC_CH_RXFIFO_Configure(MAXBOTIX_USIC, 32, XMC_USIC_CH_FIFO_SIZE_32WORDS, 0);

	// Set service request for rx FIFO receive interrupt
	XMC_USIC_CH_RXFIFO_SetInterruptNodePointer(MAXBOTIX_USIC, XMC_USIC_CH_RXFIFO_INTERRUPT_NODE_POINTER_STANDARD, MAXBOTIX_SERVICE_REQUEST_RX);
	XMC_USIC_CH_RXFIFO_SetInterruptNodePointer(MAXBOTIX_USIC, XMC_USIC_CH_RXFIFO_INTERRUPT_NODE_POINTER_ALTERNATE, MAXBOTIX_SERVICE_REQUEST_RX);

	// Set priority and enable NVIC node for receive interrupt
	NVIC_SetPriority((IRQn_Type)MAXBOTIX_IRQ_RX, MAXBOTIX_IRQ_RX_PRIORITY);
	NVIC_EnableIRQ((IRQn_Type)MAXBOTIX_IRQ_RX);

	// Start UART
	XMC_UART_CH_Start(MAXBOTIX_USIC);

	XMC_USIC_CH_EnableEvent(MAXBOTIX_USIC, XMC_USIC_CH_EVENT_ALTERNATIVE_RECEIVE);
	XMC_USIC_CH_RXFIFO_EnableEvent(MAXBOTIX_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);
}

void maxbotix_init(void) {
	memset(&maxbotix, 0, sizeof(Maxbotix));

	maxbotix.enable = true;
	XMC_GPIO_CONFIG_t pin_config = {
		.mode             = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH
	};

	XMC_GPIO_Init(MAXBOTIX_ENABLE_PIN, &pin_config);

	maxbotix_init_uart();
}

uint16_t maxbotix_get_distance(void) {
	return maxbotix.distance;
}