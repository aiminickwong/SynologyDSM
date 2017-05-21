#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/io.h>

#include <linux/amba/serial.h>
#include <linux/serial_reg.h>
#if defined (MY_DEF_HERE)
#include <linux/st-asc.h>
#endif  

static void __iomem *early_base;
static void (*printch)(char ch);

static void pl011_printch(char ch)
{
	while (readl_relaxed(early_base + UART01x_FR) & UART01x_FR_TXFF)
		;
	writeb_relaxed(ch, early_base + UART01x_DR);
	while (readl_relaxed(early_base + UART01x_FR) & UART01x_FR_BUSY)
		;
}

static void smh_printch(char ch)
{
	asm volatile("mov  x1, %0\n"
		     "mov  x0, #3\n"
		     "hlt  0xf000\n"
		     : : "r" (&ch) : "x0", "x1", "memory");
}

static void uart8250_8bit_printch(char ch)
{
	while (!(readb_relaxed(early_base + UART_LSR) & UART_LSR_THRE))
		;
	writeb_relaxed(ch, early_base + UART_TX);
}

static void uart8250_32bit_printch(char ch)
{
	while (!(readl_relaxed(early_base + (UART_LSR << 2)) & UART_LSR_THRE))
		;
	writel_relaxed(ch, early_base + (UART_TX << 2));
}

#if defined (MY_DEF_HERE)
 
static void st_asc_printch(char ch)
{
	while (!(readl_relaxed(early_base + ASC_STA) & ASC_STA_TE))
		;
	writeb_relaxed(ch, early_base + ASC_TXBUF);
	while (readl_relaxed(early_base + ASC_STA) & ASC_STA_TF)
		;
}
#endif  

struct earlycon_match {
	const char *name;
	void (*printch)(char ch);
};

static const struct earlycon_match earlycon_match[] __initconst = {
	{ .name = "pl011", .printch = pl011_printch, },
	{ .name = "smh", .printch = smh_printch, },
	{ .name = "uart8250-8bit", .printch = uart8250_8bit_printch, },
	{ .name = "uart8250-32bit", .printch = uart8250_32bit_printch, },
#if defined (MY_DEF_HERE)
	{ .name = "st-asc", .printch = st_asc_printch, },
#endif  
	{}
};

static void early_write(struct console *con, const char *s, unsigned n)
{
	while (n-- > 0) {
		if (*s == '\n')
			printch('\r');
		printch(*s);
		s++;
	}
}

static struct console early_console_dev = {
	.name =		"earlycon",
	.write =	early_write,
	.flags =	CON_PRINTBUFFER | CON_BOOT,
	.index =	-1,
};

static int __init setup_early_printk(char *buf)
{
	const struct earlycon_match *match = earlycon_match;
	phys_addr_t paddr = 0;

	if (!buf) {
		pr_warning("No earlyprintk arguments passed.\n");
		return 0;
	}

	while (match->name) {
		size_t len = strlen(match->name);
		if (!strncmp(buf, match->name, len)) {
			buf += len;
			break;
		}
		match++;
	}
	if (!match->name) {
		pr_warning("Unknown earlyprintk arguments: %s\n", buf);
		return 0;
	}

	if (!strncmp(buf, ",0x", 3)) {
		char *e;
		paddr = simple_strtoul(buf + 1, &e, 16);
		buf = e;
	}
	 
	if (paddr)
		early_base = early_io_map(paddr, EARLYCON_IOBASE);

	printch = match->printch;
	early_console = &early_console_dev;
	register_console(&early_console_dev);

	return 0;
}

early_param("earlyprintk", setup_early_printk);
