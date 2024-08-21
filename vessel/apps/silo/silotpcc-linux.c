#include "common.h"
#include "silotpcc.h"
#include <stdlib.h>
#include <stdio.h>

size_t memory;

void process_request(void)
{
	silotpcc_exec_one(thread_no);
}

void init_thread(void)
{
	silotpcc_init_thread(thread_no);
}


static void help(const char *prgname)
{
	printf("Usage: %s n_cpu port memory\n", prgname);
}

int main(int argc, char *argv[])
{
	int n_cpu, port;
	if (argc < 4) {
		help(argv[0]);
		return -1;
	}

	n_cpu = atoi(argv[1]);
	port = atoi(argv[2]);
	memory = atoll(argv[3]);
	init_linux(n_cpu, port);
	silotpcc_init(n_cpu, memory);
	start_linux_server();

	return 0;
}
