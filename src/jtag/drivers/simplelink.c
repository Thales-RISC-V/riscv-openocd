#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jtag/interface.h>
#include "bitbang.h"
#include <sys/mman.h>

#define JTAG_BASEADDR 0x43C00000
#define JTAG_SIZE 0xFFFF

struct simplelink_regs {
	uint32_t tck;
	uint32_t tms;
	uint32_t tdi;
	uint32_t tdo;
	uint32_t trst;
};

static struct simplelink_regs *regs;
static uint32_t simplelink_baseaddr = JTAG_BASEADDR;

COMMAND_HANDLER(simplelink_handle_baseaddr)
{
	if (CMD_ARGC != 1) {
		command_print(CMD_CTX, "Need exactly one argument for SimpleLink baseaddr.");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	if (sscanf(CMD_ARGV[0], "0x%08x", &simplelink_baseaddr) != 1) {
		command_print(CMD_CTX, "Invalid SimpleLink baseaddr: %s.", CMD_ARGV[0]);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static const struct command_registration simplelink_command_handlers[] = {
	{
		.name = "simplelink_baseaddr",
		.handler = &simplelink_handle_baseaddr,
		.mode = COMMAND_CONFIG,
		.help = "baseaddr of the SimpleLink ip-core",
		.usage = "e.g 0x43c00000",
	},
	COMMAND_REGISTRATION_DONE
};

static int simplelink_read(void)
{
	return (regs->tck & 0x01);
}
static void simplelink_write(int tck, int tms, int tdi)
{
	uint32_t reg = 0;
	reg = ((tck & 0x1) << 31) | ((tms & 0x1) << 30) | ((tdi & 0x1) << 29);
	regs->tck = reg;
}

static void simplelink_reset(int trst, int srst)
{
	if (trst)
		regs->tck |= (1 << 28);
	else
		regs->tck &=  ~(1<<28) ;
}

static int simplelink_init(void);
static int simplelink_quit(void);

static const char * const simplelink_transports[] = { "jtag", NULL };

struct jtag_interface simplelink_interface = {
	.name = "simplelink",
	.supported = DEBUG_CAP_TMS_SEQ,
	.execute_queue = bitbang_execute_queue,
	.transports = simplelink_transports,
	.swd = &bitbang_swd,
	.commands = simplelink_command_handlers,
	.init = simplelink_init,
	.quit = simplelink_quit,
};

static struct bitbang_interface simplelink_bitbang = {
	.read = simplelink_read,
	.write = simplelink_write,
	.reset = simplelink_reset,
	.swdio_read = NULL,
	.swdio_drive = NULL,
	.blink = 0
};

static int simplelink_clean(void) {
	if (!regs) {
		return ERROR_OK; /* not mapped */
	}

	munmap(regs, JTAG_SIZE);
	return ERROR_OK;
}

static int simplelink_init(void)
{
	bitbang_interface = &simplelink_bitbang;
	int fd;
	void *addr;

	LOG_INFO("SimpleLink FPGA JTAG ip-core driver");
	fd = open("/dev/mem",  O_RDWR | O_SYNC);

	if (fd < 0) {
		printf("Error opening file [%d]: %s \n",errno, strerror(errno) );
		goto out_error;
	}
	addr = mmap(NULL, JTAG_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, simplelink_baseaddr);

	if (!addr)
		goto out_error;

	/* Assign it to the struct */
	regs = (struct simplelink_regs*) addr;

	LOG_INFO("SimpleLink initialization complete");
	close(fd);
	return ERROR_OK;

out_error:
	LOG_INFO("SimpleLink initialization error");
	close(fd);
	simplelink_clean();
	return ERROR_JTAG_INIT_FAILED;
}

static int simplelink_quit(void)
{
	return simplelink_clean();
}
