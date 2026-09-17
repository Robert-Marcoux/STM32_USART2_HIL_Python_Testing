
/* USART2 VCP with Polling — Instrumented v1
 * Same polling echo behavior as USART2_VCP_Polling, with SysTick added
 * to measure elapsed time and report CPU busy time to the host test
 * suite via a query byte ('?'). For polling, busy time equals elapsed
 * time by construction, since the loop never idles. See project README. */

#include <stdint.h>

/* Relevant Base Addresses and Offsets */

/* RCC - Reset and Clock Control */
#define RCC_BASE 		0x40021000UL
#define RCC_CR		(*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_CFGR		(*(volatile uint32_t *)(RCC_BASE + 0x08))
#define RCC_IOPENR 		(*(volatile uint32_t *)(RCC_BASE + 0x34))
#define RCC_APBENR1		(*(volatile uint32_t *)(RCC_BASE + 0x3C))
#define RCC_CCIPR		(*(volatile uint32_t *)(RCC_BASE + 0x54))

/* ARM SysTick - Core Timer Added Instrumentation */
#define SYST_BASE		0xE000E010UL
#define SYST_CSR		(*(volatile uint32_t *)(SYST_BASE + 0x00))
#define SYST_RVR		(*(volatile uint32_t *)(SYST_BASE + 0x04))
#define QUERY_BYTE		0X3F /* ascii ‘?’ for query and testing */
/* GPIOA - General Purpose I/O for Port A */
#define GPIOA_BASE		0x50000000UL
#define GPIOA_MODER		(*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL  	(*(volatile uint32_t *)(GPIOA_BASE + 0x20))

/* USART2 */
#define USART2_BASE		0x40004400UL
#define USART2_CR1		(*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_BRR		(*(volatile uint32_t *)(USART2_BASE + 0x0C))
#define USART2_ISR		(*(volatile uint32_t *)(USART2_BASE + 0x1C))
#define USART2_RDR		(*(volatile uint32_t *)(USART2_BASE + 0x24))
#define USART2_TDR		(*(volatile uint32_t *)(USART2_BASE + 0x28))

/* USART2_ISR receive and transfer status bit locations */
#define RXNE_BIT 5
#define TXE_BIT  7

#define BAUD_RATE 9600UL


/* SysTick counter and core exception handler */
volatile uint32_t tick_count = 0;

void SysTick_Handler(void) {
    tick_count++;
}

int main(void)
{
	/* Enable RCC clocks for GPIOA and USART2 */
	RCC_IOPENR = RCC_IOPENR |    (1UL << 0);
	RCC_APBENR1 = RCC_APBENR1 | (1UL << 17);

	/* SysTick CSR Init */
	SYST_CSR = SYST_CSR & ~(0x7 << 0);
	SYST_CSR = SYST_CSR |  (0x6 << 0);

	/* Set PA2 and PA3 modes to alternate function for USART2 */
	GPIOA_MODER = GPIOA_MODER & ~(0xF << 4);
	GPIOA_MODER = GPIOA_MODER |  (0xA << 4);
	GPIOA_AFRL = GPIOA_AFRL & ~(0xFF << 8);
	GPIOA_AFRL = GPIOA_AFRL |  (0x11 << 8);

	/* USART - bit clear and set TE and RE bits to enable the transmitter and receiver */
	USART2_CR1 = USART2_CR1 & ~(0xC << 0);
	USART2_CR1 = USART2_CR1 |  (0xC << 0);


	/* HSI, AHB AND APB clock frequency divisor check and clock adjustment */
	/* This adjusted clock frequency is used to set the Baud Rate value in USART_BRR */

	/* HSI48 clock divisor check */
	uint32_t hsidiv_bits = (RCC_CR >> 11) & 0x07;
	uint32_t HSI_divisor = (1UL << hsidiv_bits);


	/* AHB Prescaler check */

	uint32_t ahbdiv_bits;
	uint32_t AHB_divisor;

	if((RCC_CFGR & 0x00000800) == 0)
	{
		AHB_divisor = 1;
	}
	else
	{
		ahbdiv_bits = (RCC_CFGR >> 8) & 0X07;
		if (ahbdiv_bits <= 3)
			AHB_divisor = (1UL << (ahbdiv_bits + 1));
		else
			AHB_divisor = (1UL << (ahbdiv_bits + 2));
	}


	/* APB Prescaler check */

	uint32_t apbdiv_bits;
	uint32_t APB_divisor;

	if((RCC_CFGR & 0x00004000) == 0)
	{
		APB_divisor = 1;
	}
	else
	{
		apbdiv_bits = (RCC_CFGR >> 12) & 0X03;
		APB_divisor = (1UL << (apbdiv_bits + 1));
	}

	uint32_t total_divisor = (HSI_divisor * AHB_divisor * APB_divisor);

	/* Adjust APB clock to reflect the sum of the clock divisors */
	uint32_t APB_clk_adjust = (48000000UL / total_divisor);

	/* Set baud rate register prior to enabling USART */
	USART2_BRR = APB_clk_adjust / BAUD_RATE;

	/* Set SysTick reload value to roll over every 1 ms */
	SYST_RVR = ((APB_clk_adjust/1000) - 1);

	/* Enable the SysTick timer */
	SYST_CSR = SYST_CSR |  (0x1 << 0);

          /* Clear and set the UE bit to USART enable */
	USART2_CR1 = USART2_CR1 & ~(0x1 << 0);
	USART2_CR1 = USART2_CR1 |  (0x1 << 0);

	/* variable that is assigned data received on the VCP from the test script */
	uint32_t received_data;

	for (;;)
	{
		if ((USART2_ISR & (1UL << RXNE_BIT)) != 0)		  /* Data received and ready to be read */
		{
			received_data = USART2_RDR;                        		  /* Read data from the RDR */

            /* added instrumentation returns SysTick’s tick_count when requested by test script */
			if (received_data == QUERY_BYTE)               		    /* request from test script */
			{
	    			uint8_t t0 =  tick_count        & 0xFF;   	/* LSB — sent first */
	    			uint8_t t1 = (tick_count >> 8)  & 0xFF;
	    			uint8_t t2 = (tick_count >> 16) & 0xFF;
	    			uint8_t t3 = (tick_count >> 24) & 0xFF;		/* MSB — sent last */



				/* allocate 64 bits for the busy tick counter and truncate to avoid roll over */
	    			uint64_t busy_ticks_wide = (uint64_t)tick_count * 12000ULL;
	    			uint32_t busy_ticks = (uint32_t)busy_ticks_wide;

		    		uint8_t b0 =  busy_ticks        & 0xFF;		/* LSB — sent first */
	    			uint8_t b1 = (busy_ticks >> 8)  & 0xFF;
		    		uint8_t b2 = (busy_ticks >> 16) & 0xFF;
	    			uint8_t b3 = (busy_ticks >> 24) & 0xFF;		/* MSB — sent last */

				/* check transmit register and transmit bytes back to script */

	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    				USART2_TDR = t0;
	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    				USART2_TDR = t1;
	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    				USART2_TDR = t2;
	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    				USART2_TDR = t3;

	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    			    USART2_TDR = b0;
	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    			    USART2_TDR = b1;
	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    			    USART2_TDR = b2;
	    			while ((USART2_ISR & (1UL << TXE_BIT)) == 0) { }
	    			    USART2_TDR = b3;
			}
			else	/* normal echoing of received bit */
			{
				while ((USART2_ISR & (1UL << TXE_BIT)) == 0)	/* Transmit register is full */
				{
					/* wait for TDR to empty and be ready to receive data */
				}
				USART2_TDR = received_data; 			/* Write receieved_data into the TDR */
			}
		}
	}
}

