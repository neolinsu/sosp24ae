#include <linux/memfd.h>

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#include "uexec/uexec.h"
#include "base/log.h"

#include "uexec_common.h"

extern char **environ;

void reflect_mfd_execv(const unsigned char *elf, char **argv) {
	log_debug("Using default environment %p\n", (void *)environ);
	reflect_mfd_execve(elf, argv, environ);
}

void reflect_mfd_execve(const unsigned char *elf, char **argv, char **env) {
	int out, ii;
	ssize_t l = 0;
	size_t end = 0, written = 0;
	ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *) elf;
	ElfW(Phdr) *phdr = (ElfW(Phdr) *)(elf + ehdr->e_phoff);

	if (!is_compatible_elf((ElfW(Ehdr) *)elf)) {
		abort();
	}


	for(ii = 0; ii < ehdr->e_phnum; ii++, phdr++) {
		if(phdr->p_type == PT_LOAD) {
			if (end < phdr->p_offset + phdr->p_filesz) {
				end = phdr->p_offset + phdr->p_filesz;
			}
		}
	}

	out = syscall(SYS_memfd_create, "", MFD_CLOEXEC);
	BUG_ON(ftruncate(out, end));

	while (written < end) {
		l = write(out, elf + written, end - written);
		if (l == -1) {
		  log_debug("Failed to write: %s\n", strerror(errno));
		  abort();
		}
		written += l;
	}

	syscall(SYS_execveat, out, "", argv, env, AT_EMPTY_PATH);
}