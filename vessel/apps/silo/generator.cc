#include "generator.h"

static Generator *generator;

void create_generator(const char *str)
{
	generator = createGenerator(str);
}

double generate(void)
{
	return generator->generate();
}

void log_file_line(log_level_t, char const*, int, char const*, ...)
{
}
