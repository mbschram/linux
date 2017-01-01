#ifndef __ASM_KASAN_H
#define __ASM_KASAN_H

#ifndef __ASSEMBLY__

#ifdef CONFIG_KASAN

#include <linux/linkage.h>
#include <asm/memory.h>
#include <asm/pgtable-types.h>

/*
 * KASAN_SHADOW_START: beginning of the kernel virtual addresses.
 * KASAN_SHADOW_END: KASAN_SHADOW_START + 1/8 of the kernel virtual addresses.
 */
#define KASAN_SHADOW_START	(VMALLOC_START)
#define KASAN_SHADOW_END	(KASAN_SHADOW_START + KASAN_SHADOW_SIZE)

/* This value is used to map an address to the corresponding shadow
 * address by the following formula:
 *	shadow_addr = (address >> 3) + KASAN_SHADOW_OFFSET;
 *
 */
#define KASAN_SHADOW_OFFSET	(KASAN_SHADOW_END - \
				 (PAGE_OFFSET - (1ULL << 3)))
				 

void kasan_init(void);
void kasan_copy_shadow(pgd_t *pgdir);
asmlinkage void kasan_early_init(void);

#else
static inline void kasan_init(void) { }
static inline void kasan_copy_shadow(pgd_t *pgdir) { }
#endif /* CONFIG_KASAN */

#endif /* __ASSEMBLY__ */

#endif /* __ASM_KASAN_H */
