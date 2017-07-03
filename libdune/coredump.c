// #include <fcntl.h>

// #include "dune.h"

// #define ELF_EXEC_PAGESIZE 4096
// #define ULONG_MAX	(~0UL)

// struct core_mm_vma {
// 	struct core_mm_vma *next;
// }

// struct core_mm {
// 	int map_count;
// 	struct core_mm_vma* vma;
// 	struct core_mm_vma* last_vma;
// };

// static struct core_mm mm = {
// 	.map_count = 0
// 	.vma = NULL
// 	.last_vma = NULL
// };

// static inline void core_mm_push_vma(struct core_mm *mm, struct core_mm_vma *vma)
// {
// 	if (mm->last_vma == NULL) {
// 		mm->vma = vma;
// 	} else {
// 		mm->last_vma->next = vma;
// 	}
// 	mm->map_count++;
// 	mm->last_vma = vma;
// 	vma->next = NULL;
// }

// static void core_mm_iterator(const struct dune_procmap_entry *ent) {
// 	struct core_mm_vma* vma = malloc(sizeof(struct core_mm_vma)); // TODO free
// 	core_mm_push_vma(&mm, vma);
// }

// static void fill_elf_header(Elf64_Ehdr *elf, int segs, u16 machine, u32 flags)
// {
// 	memset(elf, 0, sizeof(*elf));

// 	memcpy(elf->e_ident, ELFMAG, SELFMAG);
// 	elf->e_ident[EI_CLASS] = ELFCLASS64;
// 	elf->e_ident[EI_DATA] = ELFDATA2LSB;
// 	elf->e_ident[EI_VERSION] = EV_CURRENT;
// 	elf->e_ident[EI_OSABI] = ELFOSABI_LINUX;

// 	elf->e_type = ET_CORE;
// 	elf->e_machine = machine;
// 	elf->e_version = EV_CURRENT;
// 	elf->e_phoff = sizeof(Elf64_Ehdr);
// 	elf->e_flags = flags;
// 	elf->e_ehsize = sizeof(Elf64_Ehdr);
// 	elf->e_phentsize = sizeof(Elf64_Phdr);
// 	elf->e_phnum = segs;

// 	return;
// }

// /*
//  * The purpose of always_dump_vma() is to make sure that special kernel mappings
//  * that are useful for post-mortem analysis are included in every core dump.
//  * In that way we ensure that the core dump is fully interpretable later
//  * without matching up the same kernel and hardware config to see what PC values
//  * meant. These special mappings include - vDSO, vsyscall, and other
//  * architecture specific mappings
//  */
// static bool always_dump_vma(struct vm_area_struct *vma)
// {
// 	/* Any vsyscall mappings? */
// 	// if (vma == get_gate_vma(vma->vm_mm))
// 	// 	return true;

// 	/*
// 	 * Assume that all vmas with a .name op should always be dumped.
// 	 * If this changes, a new vm_ops field can easily be added.
// 	 */
// 	if (vma->vm_ops && vma->vm_ops->name && vma->vm_ops->name(vma))
// 		return true;

// 	/*
// 	 * arch_vma_name() returns non-NULL for special architecture mappings,
// 	 * such as vDSO sections.
// 	 */
// 	if (arch_vma_name(vma))
// 		return true;

// 	return false;
// }

// /*
//  * Decide what to dump of a segment, part, all or none.
//  */
// static unsigned long vma_dump_size(struct vm_area_struct *vma,
// 				   unsigned long mm_flags)
// {
// #define FILTER(type)	(mm_flags & (1UL << MMF_DUMP_##type))

// 	/* always dump the vdso and vsyscall sections */
// 	if (always_dump_vma(vma))
// 		goto whole;

// 	if (vma->vm_flags & VM_DONTDUMP)
// 		return 0;

// 	/* support for DAX */
// 	if (vma_is_dax(vma)) {
// 		if ((vma->vm_flags & VM_SHARED) && FILTER(DAX_SHARED))
// 			goto whole;
// 		if (!(vma->vm_flags & VM_SHARED) && FILTER(DAX_PRIVATE))
// 			goto whole;
// 		return 0;
// 	}

// 	/* Hugetlb memory check */
// 	if (vma->vm_flags & VM_HUGETLB) {
// 		if ((vma->vm_flags & VM_SHARED) && FILTER(HUGETLB_SHARED))
// 			goto whole;
// 		if (!(vma->vm_flags & VM_SHARED) && FILTER(HUGETLB_PRIVATE))
// 			goto whole;
// 		return 0;
// 	}

// 	/* Do not dump I/O mapped devices or special mappings */
// 	if (vma->vm_flags & VM_IO)
// 		return 0;

// 	/* By default, dump shared memory if mapped from an anonymous file. */
// 	if (vma->vm_flags & VM_SHARED) {
// 		if (file_inode(vma->vm_file)->i_nlink == 0 ?
// 		    FILTER(ANON_SHARED) : FILTER(MAPPED_SHARED))
// 			goto whole;
// 		return 0;
// 	}

// 	/* Dump segments that have been written to.  */
// 	if (vma->anon_vma && FILTER(ANON_PRIVATE))
// 		goto whole;
// 	if (vma->vm_file == NULL)
// 		return 0;

// 	if (FILTER(MAPPED_PRIVATE))
// 		goto whole;

// 	/*
// 	 * If this looks like the beginning of a DSO or executable mapping,
// 	 * check for an ELF header.  If we find one, dump the first page to
// 	 * aid in determining what was mapped here.
// 	 */
// 	if (FILTER(ELF_HEADERS) &&
// 	    vma->vm_pgoff == 0 && (vma->vm_flags & VM_READ)) {
// 		u32 __user *header = (u32 __user *) vma->vm_start;
// 		u32 word;
// 		mm_segment_t fs = get_fs();
// 		/*
// 		 * Doing it this way gets the constant folded by GCC.
// 		 */
// 		union {
// 			u32 cmp;
// 			char elfmag[SELFMAG];
// 		} magic;
// 		BUILD_BUG_ON(SELFMAG != sizeof word);
// 		magic.elfmag[EI_MAG0] = ELFMAG0;
// 		magic.elfmag[EI_MAG1] = ELFMAG1;
// 		magic.elfmag[EI_MAG2] = ELFMAG2;
// 		magic.elfmag[EI_MAG3] = ELFMAG3;
// 		/*
// 		 * Switch to the user "segment" for get_user(),
// 		 * then put back what elf_core_dump() had in place.
// 		 */
// 		set_fs(USER_DS);
// 		if (unlikely(get_user(word, header)))
// 			word = 0;
// 		set_fs(fs);
// 		if (word == magic.cmp)
// 			return PAGE_SIZE;
// 	}

// #undef	FILTER

// 	return 0;

// whole:
// 	return vma->vm_end - vma->vm_start;
// }

// /*
//  * Dune core dumper build upon linux's elf_core_dump as a template.
//  *
//  * Actual dumper
//  *
//  * This is a two-pass process; first we find the offsets of the bits,
//  * and then they are actually written out.  If we run out of core limit
//  * we just truncate.
//  */
// int dune_core_dump(struct coredump_params *cprm)
// {
// 	int has_dumped = 0;
// 	int segs;
// 	size_t offset = 0;
// 	Elf64_Ehdr elf;
// 	Elf64_Addr *vma_filesz = NULL;
// 	struct core_mm_vma *vma;

// 	/*
// 	 * The number of segs are recored into ELF header as 16bit value.
// 	 * Please check DEFAULT_MAX_MAP_COUNT definition when you modify here.
// 	 */
// 	dune_procmap_iterate(core_mm_iterator);

// 	segs = mm.map_count;
// 	// segs += elf_core_extra_phdrs();

// 	// gate_vma = get_gate_vma(current->mm);
// 	// if (gate_vma != NULL)
// 	// 	segs++;

// 	/* for notes section */
// 	// segs++;

// 	// if (!fill_note_info(elf, e_phnum, &info, cprm->siginfo, cprm->regs))
// 	// 	goto cleanup;
// 	fill_elf_header(&elf, segs, EM_X86_64, 0);

// 	has_dumped = 1;

// 	// fs = get_fs();
// 	// set_fs(KERNEL_DS);

// 	offset += sizeof(elf);				/* Elf header */
// 	offset += segs * sizeof(Elf64_Phdr);	/* Program headers */

// 	/* Write notes phdr entry */
// 	// {
// 	// 	size_t sz = get_note_info_size(&info);

// 	// 	sz += elf_coredump_extra_notes_size();

// 	// 	phdr4note = kmalloc(sizeof(*phdr4note), GFP_KERNEL);
// 	// 	if (!phdr4note)
// 	// 		goto end_coredump;

// 	// 	fill_elf_note_phdr(phdr4note, sz, offset);
// 	// 	offset += sz;
// 	// }

// 	dataoff = offset = roundup(offset, ELF_EXEC_PAGESIZE);

// 	if (segs - 1 > ULONG_MAX / sizeof(*vma_filesz))
// 		goto end_coredump;
// 	vma_filesz = malloc((segs - 1) * sizeof(*vma_filesz));
// 	if (!vma_filesz)
// 		goto end_coredump;

// 	for (i = 0, vma = mm.vma; vma != NULL; vma = vma->next) {
// 		unsigned long dump_size;

// 		dump_size = vma_dump_size(vma, cprm->mm_flags);
// 		vma_filesz[i++] = dump_size;
// 		vma_data_size += dump_size;
// 	}

// 	return 0;
// }

#include <linux/fs.h>
#include <sys/ioctl.h>
#include "dune.h"

extern int dune_fd;
extern struct dune_config *global_conf;

int dune_core_dump()
{
	if (global_conf == NULL) {
		return -1;
	}
	return ioctl(dune_fd, DUNE_COREDUMP, global_conf);
}