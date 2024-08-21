#include "common.h"
#include "silotpcc.h"
#include <stdlib.h>
#include <stdio.h>

void process_request(void)
{
	silotpcc_exec_one(thread_no);
}

int nrcpus;
size_t memory;

int init_global(void)
{
	silotpcc_init(nrcpus, memory);
	return 0;
}

void init_thread(void)
{
	silotpcc_init_thread(thread_no);
}

void usage(const char *p)
{
	fprintf(stderr, "usage: %s cfgfile nthreads port memory\n", p);
}

int main(int argc, char *argv[])
{
	int port;

	if (argc < 5) {
		usage(argv[0]);
		return -1;
	}

	nrcpus = atoi(argv[2]);
	port = atoi(argv[3]);  
	memory = atoll(argv[4]);

	return init_shenango(argv[1], port);
}
