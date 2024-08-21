#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "base/log.h"
#include "core/mem.h"
#include "core/cluster.h"
#include "core/kthread.h"
#include "uexec/uexec.h"

#include "uexec_common.h"

extern char **environ;
char *ld_path;

__noreturn void reflect_execv(const unsigned char *elf, char **argv, uid_t user_id) {
	log_debug("Using default environment %p\n", (void *)environ);
	reflect_execve(elf, argv, NULL, user_id);
}

__noreturn void reflect_execve(const unsigned char *elf, char **argv, char **env, uid_t user_id) {
	// When allocating a new stack, be sure to give it lots of space since the OS
	// won't always honor MAP_GROWSDOWN
	// TODO: Stack Size
	cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;
	size_t *new_stack = (size_t *) (2047 * PAGE_SIZE +  (char *) ((aligned_alloc_func_t)(cur->minimal_ops.aligned_alloc))(PAGE_SIZE, 2048 * PAGE_SIZE));
		//mmap(0, 2048 * PAGE_SIZE,
		//PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_GROWSDOWN, -1, 0));

	log_debug("Allocated new stack %p\n", (void *)new_stack);
	reflect_execves(elf, argv, env, new_stack, user_id);
}

__noreturn void reflect_execves(const unsigned char *elf, char **argv, char **env, size_t *stack, uid_t  user_id) {
	int fd;
	struct stat statbuf;
	unsigned char *data = NULL, *src=NULL;
	size_t argc;
	struct mapped_elf exe = {0}, interp = {0};
	cluster_ctx_t* cur = (cluster_ctx_t*) cur_cluster;

	if (!is_compatible_elf((ElfW(Ehdr) *)elf)) {
		abort();
	}

	if (env == NULL) {
		env = environ;
	}
	map_elf(elf, &exe);
	if (exe.ehdr == NULL) {
		log_err("Unable to map ELF file: %s\n", strerror(errno));
		abort();
	}
	if (exe.interp) {
		// Load input ELF executable into memory
		// fd = open(exe.interp, O_RDONLY);
		fd = open(ld_path, O_RDONLY); // TODO
		log_debug("%s\n", ld_path);

		if(fd == -1) {
			log_err("Failed to open loader for %s\n", strerror(errno));
			abort();
		}

		if(fstat(fd, &statbuf) == -1) {
			log_err("Failed to fstat(fd): %s\n", strerror(errno));
			abort();
		}
		
		data = ((aligned_alloc_func_t)(cur->minimal_ops.aligned_alloc))(ALIGN_SIZE, (statbuf.st_size)); //mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (data==NULL) {
			log_err("Fail to alloc memory for interp.");
			abort();
		}
		log_debug("interpreter begins at %p\n", data);
		log_debug("statbuf.st_size = %lu\n", statbuf.st_size);
		src = (unsigned char*) mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (src== MAP_FAILED) {
			log_err("Fail to mmap interp for %s.", strerror(errno));
			abort();
		}
		memcpy((void*)data, (void*)src, statbuf.st_size);
		munmap(src, statbuf.st_size);
		if(data == NULL) {
			log_err("Unable to read ELF file in: %s\n", strerror(errno));
			abort();
		}
		close(fd);

		map_elf(data, &interp);
		if (interp.ehdr == NULL) {
			log_err("Unable to map interpreter for ELF file: %s\n", strerror(errno));
			abort();
		}
	} else {
		interp = exe;
	}
	log_info("exe.ehdr:%p", exe.ehdr);
	for (argc = 0; argv[argc]!=NULL; argc++);
	stack_setup(stack, argc, argv, env, NULL,
			exe.ehdr, interp.ehdr);
	//ret = setuid(user_id);
	//if(ret) {
	//	log_err("Fail to set user id (%d), for %s.", user_id, strerror(errno));
	//}
	log_info("Enter User App.");
	jump_with_stack(interp.entry_point, stack);
	abort();

}