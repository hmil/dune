#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <err.h>

#include "libdune/dune.h"


void dump_core() 
{
	int ret = dune_core_dump();
	printf("status=%d\n", ret);
}

int the_loop(FILE* fin) 
{
	char buffer[20] = {0};
	
	while(fgets(buffer, 20, fin) != NULL) {
		for (char *c = buffer ; *c != '\0' ; c++) {
			if (*c == '\n') *c = '\0';
		}
		printf("Got input: %s\n", buffer);
		// This will print core
		dump_core();
	}
	return 0;
}

int main(int argc, char *argv[], char *envp[])
{
	int ret;

	if ((ret = dune_init(0)))
		return ret;
	
	if ((ret = dune_enter())) {
		printf("failed to initialize dune\n");
		return ret;
	}

	puts("## Process map:");
	// Shows the procmap for comparison with the core file
	dune_procmap_dump();
	fflush(stdout);
	puts("## End process map");
	fflush(stdout);

	return the_loop(stdin);
}
