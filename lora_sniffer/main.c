#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <peri/rcc.h>
#include <peri/gpio.h>
#include <peri/spi.h>
#include <peri/rtc.h>
#include <peri/usart1.h>
#include <np/spics.h>
#include <np/aspi.h>
#include <np/hexdump.h>
#include <ext/sx127x.h>
#include <core.h>

unsigned char rtc_enable_alarm_a = 0;

#define LORACS_PORT GPIOA
#define LORACS_PIN  4

GP_DEFINE(led, GPIOC, 13);
GP_DEFINE(lora_cs, LORACS_PORT, LORACS_PIN);

struct sx127x sx;

SPICS_GPIO(lora_ss, LORACS_PORT, LORACS_PIN);
ASPI(lora_cc, spi1, &lora_ss);

static void putstr(const char* str)
{
	while(*str)
	{
		usart1_putchar(*str++);
	}
}

void DUMP(const void* const data, const size_t size)
{
	hexdump_np(usart1_putchar, data, size,
		HEXDUMP_OPT_ASCII|HEXDUMP_OPT_ADDRESS|HEXDUMP_OPT_RELADDR);
}

int main(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
	asm("dsb");

	GP_CONFIG(lora_cs, GP_CONF_DIR_OUTPUT|GP_CONF_OUT_PUSHPULL|GP_CONF_OSPEED_SLOW);
	GP_CONFIG(led, GP_CONF_DIR_OUTPUT|GP_CONF_OUT_PUSHPULL|GP_CONF_OSPEED_FAST);

	usart1_init(115200);
	putstr("\r\n");
	spi1_init(200*1000);

	putstr("Init LoRa...\r\n");

	sx127x_init(&sx, &lora_cc);
	sx127x_set_freq(&sx, 433000000);
	sx127x_set_bandwidth(&sx, SX127X_BW_500000);
	sx127x_set_codingrate(&sx, SX127X_CR_4_8);
	sx127x_set_spreadfact(&sx, SX127X_SF_1024);
	sx127x_set_crcon(&sx, 1);
	sx127x_set_symbtimo(&sx, 1023);
	sx127x_set_syncword(&sx, 0x23);
	sx127x_set_mode(&sx, SX127X_OPMODE_MODE_STDBY);
	sx127x_start(&sx);

	char buf[16];

	{
		uint8_t m = 0;
		uint8_t v = 0;
		if(0!=sx127x_read_reg(&sx, SX127X_REG_OPMODE, &m)
			|| 0!=sx127x_read_reg(&sx, SX127X_REG_VERSION, &v))
		{
			putstr("FAILED TO INIT!\r\n");
			for(;;) { system_halt(); }
		}

		putstr("LoRa init done, ver: 0x");
		putstr(itoa16_np(v, buf));
		putstr(", mode: 0x");
		putstr(itoa16_np(m, buf));
		putstr("\r\n");
	}

	uint8_t pkt[256];
	int i = 0;

	for(;;)
	{
		i ^= 1;

		if(i) { GP_HIGH(led); }
		else  { GP_LOW(led); }

		while(0!=sx127x_waitrecv(&sx, 100*1000)) { }

		ssize_t len = sx127x_recv(&sx, pkt, sizeof(pkt));

		if(0<len)
		{
			putstr("RX ");
			putstr(itoa10_np(len, buf));
			putstr(" (0x");
			putstr(itoa16_np(len, buf));
			putstr(") octets\r\n");
			DUMP(pkt, len);
		}
	}

	return 0;
}
