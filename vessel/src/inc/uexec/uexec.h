#pragma once

#include <elf.h>
#include <link.h>
#include <stdbool.h>

/*
 * High-level interface
 */

void reflect_execves(const unsigned char *elf, char **argv, char **env, size_t *stack, uid_t user_id);

void reflect_execv(const unsigned char *elf, char **argv, uid_t user_id);
void reflect_execve(const unsigned char *elf, char **argv, char **env, uid_t user_id);

/*
 * Force using memfd_create/execveat fallback
 */
void reflect_mfd_execv(const unsigned char *elf, char **argv);
void reflect_mfd_execve(const unsigned char *elf, char **argv, char **env);


/*
 * ELF mapping interface
 */
struct mapped_elf {
	ElfW(Ehdr) *ehdr;
	ElfW(Addr) entry_point;
	char *interp;
};

void map_elf(const unsigned char *data, struct mapped_elf *obj);

bool is_compatible_elf(const ElfW(Ehdr) *ehdr);

/*
 * Stack creation and setup interface
 */
void synthetic_auxv(size_t *auxv);
void modify_auxv(size_t *auxv, ElfW(Ehdr) *exe, ElfW(Ehdr) *interp);
void stack_setup(size_t *stack_base, int argc, char **argv, char **env, size_t *auxv,
		ElfW(Ehdr) *exe, ElfW(Ehdr) *interp);

/*
 * Custom flow control
 */

void jump_with_stack(size_t dest, size_t *stack);
