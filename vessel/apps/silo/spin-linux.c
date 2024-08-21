#include <stdio.h>
#include <string.h>

#include "common.h"
#include "generator.h"

static void spin(int usecs)
{
	long start;

	start = mytime();
	while (mytime() < start + usecs)
		asm volatile("pause");
}

void process_request(void)
{
	int svc_time;
	svc_time = generate();
	spin(svc_time);
}

void init_thread(void)
{
}

static void help(const char *prgname)
{
	printf("Usage: %s [--udp] service-time-distribution\n"
	       "\n"
	       "Distributions are specified by <distribution>[:<param1>[,...]].\n"
	       "Parameters are not required.  The following distributions are supported:\n"
	       "\n"
	       "   [fixed:]<value>              Always generates <value>.\n"
	       "   uniform:<max>                Uniform distribution between 0 and <max>.\n"
	       "   normal:<mean>,<sd>           Normal distribution.\n"
	       "   exponential:<lambda>         Exponential distribution.\n"
	       "   pareto:<loc>,<scale>,<shape> Generalized Pareto distribution.\n"
	       "   gev:<loc>,<scale>,<shape>    Generalized Extreme Value distribution.\n"
	       "   bimodal:<ratio>,<v1>,<v2>    Bimodal distribution P(v1)=ratio, P(v2)=1-ratio.\n"
	       "   file:<path>                  Draws random latencies from file.\n", prgname);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		help(argv[0]);
		return -1;
	}

	init_linux();
	create_generator(argv[1]);
	start_linux_server();

	return 0;
}
