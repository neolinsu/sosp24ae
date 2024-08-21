#pragma once
#include <stdint.h>
#include <string.h>
/*
 * The legacy fx SSE/MMX FPU state format, as saved by FXSAVE and
 * restored by the FXRSTOR instructions. It's similar to the FSAVE
 * format, but differs in some areas, plus has extensions at
 * the end for the XMM registers.
 */
struct fxregs_state {
	uint16_t			cwd; /* Control Word			*/
	uint16_t			swd; /* Status Word			*/
	uint16_t			twd; /* Tag Word			*/
	uint16_t			fop; /* Last Instruction Opcode		*/
	union {
		struct {
			uint64_t	rip; /* Instruction Pointer		*/
			uint64_t	rdp; /* Data Pointer			*/
		};
		struct {
			uint32_t	fip; /* FPU IP Offset			*/
			uint32_t	fcs; /* FPU IP Selector			*/
			uint32_t	foo; /* FPU Operand Offset		*/
			uint32_t	fos; /* FPU Operand Selector		*/
		};
	};
	uint32_t			mxcsr;		/* MXCSR Register State */
	uint32_t			mxcsr_mask;	/* MXCSR Mask		*/

	/* 8*16 bytes for each FP-reg = 128 bytes:			*/
	uint32_t			st_space[32];

	/* 16*16 bytes for each XMM-reg = 256 bytes:			*/
	uint32_t			xmm_space[64];

	uint32_t			padding[12]; // 8 * 6

	union {
		uint32_t		padding1[12]; // 8 * 6
		uint32_t		sw_reserved[12];
	};

} __attribute__((aligned(16)));

BUILD_ASSERT(sizeof(struct fxregs_state) == 512);

struct xstate_header {
	uint64_t				xfeatures;
	uint64_t				xcomp_bv;
	uint64_t				reserved[6];
} __attribute__((packed));

/*
 * This is our most modern FPU state format, as saved by the XSAVE
 * and restored by the XRSTOR instructions.
 *
 * It consists of a legacy fxregs portion, an xstate header and
 * subsequent areas as defined by the xstate header.  Not all CPUs
 * support all the extensions, so the size of the extended area
 * can vary quite a bit between CPUs.
 */
struct xregs_state {
	struct fxregs_state		i387;
	struct xstate_header		header;
	uint8_t				extended_state_area[2496];
} __attribute__ ((packed, aligned (64)));

struct fpstate {
	/* @regs: The register state union for all supported formats */
	struct xregs_state	regs;

	/* @regs is dynamically sized! Don't add anything after @regs! */
} __attribute__((aligned(64)));

BUILD_ASSERT(sizeof(struct fpstate)==3072);

static inline void fpstate_init(struct fpstate *fp) {
    fp->regs.i387.cwd = 0x037F;
    fp->regs.i387.swd = 0;
    fp->regs.i387.twd = 0;
    fp->regs.i387.fop = 0;
    fp->regs.i387.rip = 0;
    fp->regs.i387.rdp = 0;
    fp->regs.i387.mxcsr = 0x1F80;
    fp->regs.i387.mxcsr_mask = ~0;
    memset(&fp->regs.i387.st_space, 0, sizeof(fp->regs.i387.st_space));
    memset(&fp->regs.i387.xmm_space, 0, sizeof(fp->regs.i387.xmm_space));
    memset(&fp->regs.header, 0, sizeof(fp->regs.header));
    memset(&fp->regs.extended_state_area, 0, sizeof(fp->regs.extended_state_area));
}

#define REX_PREFIX	"0x48, "

/* These macros all use (%edi)/(%rdi) as the single memory argument. */
#define XSAVE		".byte " REX_PREFIX "0x0f,0xae,0x27"
#define XSAVEOPT	".byte " REX_PREFIX "0x0f,0xae,0x37"
#define XSAVEC		".byte " REX_PREFIX "0x0f,0xc7,0x27"
#define XSAVES		".byte " REX_PREFIX "0x0f,0xc7,0x2f"
#define XRSTOR		".byte " REX_PREFIX "0x0f,0xae,0x2f"
#define XRSTORS		".byte " REX_PREFIX "0x0f,0xc7,0x1f"


# define DEFINE_EXTABLE_TYPE_REG \
	".macro extable_type_reg type:req reg:req\n"						\
	".set .Lfound, 0\n"									\
	".set .Lregnr, 0\n"									\
	".irp rs,rax,rcx,rdx,rbx,rsp,rbp,rsi,rdi,r8,r9,r10,r11,r12,r13,r14,r15\n"		\
	".ifc \\reg, %%\\rs\n"									\
	".set .Lfound, .Lfound+1\n"								\
	".long \\type + (.Lregnr << 8)\n"							\
	".endif\n"										\
	".set .Lregnr, .Lregnr+1\n"								\
	".endr\n"										\
	".set .Lregnr, 0\n"									\
	".irp rs,eax,ecx,edx,ebx,esp,ebp,esi,edi,r8d,r9d,r10d,r11d,r12d,r13d,r14d,r15d\n"	\
	".ifc \\reg, %%\\rs\n"									\
	".set .Lfound, .Lfound+1\n"								\
	".long \\type + (.Lregnr << 8)\n"							\
	".endif\n"										\
	".set .Lregnr, .Lregnr+1\n"								\
	".endr\n"										\
	".if (.Lfound != 1)\n"									\
	".error \"extable_type_reg: bad register argument\"\n"					\
	".endif\n"										\
	".endm\n"

# define UNDEFINE_EXTABLE_TYPE_REG \
	".purgem extable_type_reg\n"

# define _ASM_EXTABLE_TYPE(from, to, type)			\
	" .pushsection \"__ex_table\",\"a\"\n"			\
	" .balign 4\n"						\
	" .long (" #from ") - .\n"				\
	" .long (" #to ") - .\n"				\
	" .long " __stringify(type) " \n"			\
	" .popsection\n"

# define _ASM_EXTABLE_TYPE_REG(from, to, type, reg)				\
	" .pushsection \"__ex_table\",\"a\"\n"					\
	" .balign 4\n"								\
	" .long (" #from ") - .\n"						\
	" .long (" #to ") - .\n"						\
	DEFINE_EXTABLE_TYPE_REG							\
	"extable_type_reg reg=" __stringify(reg) ", type=" __stringify(type) " \n"\
	UNDEFINE_EXTABLE_TYPE_REG						\
	" .popsection\n"

/*
 * From Linux
 * If XSAVES is enabled, it replaces XSAVEC because it supports supervisor
 * states in addition to XSAVEC.
 *
 * Otherwise if XSAVEC is enabled, it replaces XSAVEOPT because it supports
 * compacted storage format in addition to XSAVEOPT.
 *
 * Otherwise, if XSAVEOPT is enabled, XSAVEOPT replaces XSAVE because XSAVEOPT
 * supports modified optimization which is not supported by XSAVE.
 *
 * We use XSAVE as a fallback.
 *
 * The 661 label is defined in the ALTERNATIVE* macros as the address of the
 * original instruction which gets replaced. We need to use it here as the
 * address of the instruction where we might get an exception at.
 */
#define XSTATE_XSAVE(st, lmask, hmask, err) \
	asm volatile(  XSAVEOPT					\
		     "\n"							\
		     "xor %[err], %[err]\n"			\
		     : [err] "=r" (err)					\
		     : "D" (st), "m" (*st), "a" (lmask), "d" (hmask)	\
		     : "memory")

/*
 * Use XRSTOR to restore context if it is enabled. XRSTORS supports compact
 * XSAVE area format.
 */
#define XSTATE_XRESTORE(st, lmask, hmask)				\
	asm volatile(								\
				 XRSTOR						\
		     "\n"						\
		     :							\
		     : "D" (st), "m" (*st), "a" (lmask), "d" (hmask)	\
		     : "memory")
