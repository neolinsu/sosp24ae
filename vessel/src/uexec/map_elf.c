#include <elf.h>
#include <errno.h>
#include <link.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "base/log.h"
#include "core/mem.h"
#include "core/cluster.h"
#include "core/kthread.h"

#include "uexec/uexec.h"
#include "map_elf.h"


// This takes a native-sized EHDR, but the parts we check are in the
// constant-sized bit so it doesn't really matter
// TODO: multilib and ehdr->e_machine checking
bool is_compatible_elf(const ElfW(Ehdr) *ehdr) {
	return (ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
			ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
			ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
			ehdr->e_ident[EI_MAG3] == ELFMAG3 &&
			ehdr->e_ident[EI_CLASS] == ELFCLASS_NATIVE &&
			ehdr->e_ident[EI_DATA] == ELFDATA_NATIVE);
}

// Non-multilib compatible, makes a mmap(2) allocation and copy of the ELF object
//
// TODO: a version that reads the file from a stream?
void map_elf(const unsigned char *data, struct mapped_elf *obj)
{
	ElfW(Addr) dest = 0;
	ElfW(Ehdr) *ehdr;
	ElfW(Phdr) *phdr;

	unsigned char *mapping = MAP_FAILED; // target memory location
	const unsigned char *source = 0;
	size_t len, virtual_offset = 0, total_to_map = 0;
	int ii, prot;
	cluster_ctx_t * cur = (cluster_ctx_t*) cur_cluster;

	// Locate ELF program and section headers
	ehdr = (ElfW(Ehdr) *)data;
	phdr = (ElfW(Phdr) *)(data + ehdr->e_phoff);

	// Go through once to get the end so we reserve enough memory
	for(ii = 0; ii < ehdr->e_phnum; ii++, phdr++) {
		if(phdr->p_type == PT_LOAD) {
			total_to_map = ((phdr->p_vaddr + phdr->p_memsz) > total_to_map
					? phdr->p_vaddr + phdr->p_memsz
					: total_to_map);
			log_debug("total mapping is now %08zx based on %08zx seg at %p\n", total_to_map, phdr->p_memsz, (void *)phdr->p_vaddr);
		}
	}

	// Reset phdr
	phdr = (ElfW(Phdr) *)(data + ehdr->e_phoff);
	for(ii = 0; ii < ehdr->e_phnum; ii++, phdr++) {
		if(phdr->p_type == PT_LOAD) {
			if(mapping == MAP_FAILED) {
				// Setup area in memory to contain the new binary image
				if (phdr->p_vaddr != 0) {
					// The first loadable segment has an address, so we are not PIE and need to readjust our perspective
					total_to_map -= phdr->p_vaddr;
				}
				// TODO
				mapping = ((aligned_alloc_func_t)(cur->minimal_ops.aligned_alloc))(PAGE_SIZE, PAGE_CEIL(total_to_map));// mmap((void *)PAGE_FLOOR(phdr->p_vaddr), PAGE_CEIL(total_to_map), PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
				if(mapping == NULL) {
					log_err("Failed to alloc(): %s\n", strerror(errno));
					goto map_failed;
				}
				memset(mapping, 0, PAGE_CEIL(total_to_map));
				if(phdr->p_vaddr == 0) virtual_offset = (size_t) mapping;
				else {
					log_err("Uspoorted elf type, PIC PIE only.");
					abort();
				}
				obj->ehdr = (ElfW(Ehdr) *) mapping;
				obj->entry_point = virtual_offset + ehdr->e_entry;
			}
			source = data + phdr->p_offset;
			dest = virtual_offset + phdr->p_vaddr;
			len = phdr->p_filesz;
			log_info("memcpy(%p, %p, %08zx)", (void *)dest, source, len);
			memcpy((void *)dest, source, len);

			prot = (((phdr->p_flags & PF_R) ? PROT_READ : 0) |
				((phdr->p_flags & PF_W) ? PROT_WRITE: 0) |
				((phdr->p_flags & PF_X) ? PROT_EXEC : 0));

			// log_debug("PAGE_FLOOR(dest): %p, PAGE_CEIL(phdr->p_memsz): %p", (void *)PAGE_FLOOR(dest), PAGE_CEIL(phdr->p_memsz));
			if(mprotect((void *)PAGE_FLOOR(dest), PAGE_CEIL(phdr->p_memsz), prot) != 0) {
				log_err("Failed to mprotect for %s\n", strerror(errno));
				goto mprotect_failed;
			}
		} else if(phdr->p_type == PT_INTERP) {
			// Since PT_INTERP must come before any PT_LOAD segments, store the
			// offset for now and add the base mapping at the end
			obj->interp = (char *) phdr->p_offset;
		}

	}

	if(obj->interp) {
		obj->interp = (char *) mapping + (size_t) obj->interp;
	}
	return;

mprotect_failed:
	((free_func_t)(cur->minimal_ops.free))(mapping);

map_failed:
	obj->ehdr = MAP_FAILED;
}
