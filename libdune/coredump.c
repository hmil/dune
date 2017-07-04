#include <linux/fs.h>
#include <sys/ioctl.h>
#include "dune.h"

extern int dune_fd;

int dune_core_dump()
{
	return ioctl(dune_fd, DUNE_COREDUMP, NULL);
}