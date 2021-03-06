#ifndef _PAGE_H_
#define _PAGE_H_

typedef unsigned int pteval_t;
/*
 * These are used to make use of C type-checking...
 * In order to simulate the 32-bit address in 64-bit machine,
 * we use unsigned int to replace unsigned long.
 */
typedef struct { pteval_t pte;} pte_t;
typedef struct { unsigned int pmd;} pmd_t;
typedef struct { unsigned int pgd[2];} pgd_t;
typedef struct { unsigned int pgprot;} pgprot_t;

#define pgprot_val(x)  (((pgprot_t)(x)).pgprot)

#define __pte(x)      ((pte_t) {(x)})
#define __pgprot(x)   ((pgprot_t) {(x)})
#define __pmd(x)      ((pmd_t) { (x) } )

#define PAGE_SHIFT 12
#define PAGE_SIZE (unsigned int)(1UL << PAGE_SHIFT)
#define PAGE_MASK (unsigned int)(~(PAGE_SIZE - 1))

#define PAGE_ALIGN(x) (x & ~(PAGE_SIZE - 1))

#define PAGE_OFFSET (unsigned long)0xC0000000
#define PHYS_OFFSET (unsigned long)0x50000000
#define PHYS_PFN_OFFSET (unsigned long)((PHYS_OFFSET) >> PAGE_SHIFT)

typedef struct page *pgtable_t;

#define virt_to_phys(x) ((unsigned long)(x) - PAGE_OFFSET + PHYS_OFFSET)
#define phys_to_virt(x) ((unsigned long)(x) - PHYS_OFFSET + PAGE_OFFSET)
#define pfn_to_phys(x)  ((unsigned long)(x) << PAGE_SHIFT)
#define phys_to_pfn(x)  ((unsigned long)(x) >> PAGE_SHIFT)
#define virt_to_pfn(x)  phys_to_pfn(virt_to_phys(x))
#define pfn_to_virt(x)  phys_to_virt(pfn_to_phys(x))
#define __pa(x) virt_to_phys(x)
#define __va(x) phys_to_virt(x)
#define __pfn_to_phys(x) pfn_to_phys(x)
extern struct page *mem_map;
extern void *phys_to_mem(phys_addr_t addr);
/*
 * Note: In order to ignore the Virtual Memory layer we change the way
 * to get page via a pfn.Please mind the address of page is a Virtual
 * Memory address not a Physical address.
 * If you want to use a page structure,please use the macro to get 
 * page with a pfn.
 * @mem_map uses a physical address.
 * @page uses a virtual memory address.
 */
#define pfn_to_page(pfn)     \
	(struct page *)(unsigned long)phys_to_mem(     \
			((phys_addr_t)(unsigned long)(mem_map +  \
			((pfn) - PHYS_PFN_OFFSET))))
/*
 * Note: In order to ignore the Virtual Memory layer,we use the Virtual
 * Memory address for page structure.Please mind the address of mem_map
 * is a physical address when you use this value.
 */
#define page_to_pfn(x)   \
	 (unsigned long)(     \
	(unsigned long)((struct page *)(unsigned long)(phys_addr_t)mem_to_phys(x) \
		- mem_map) + PHYS_PFN_OFFSET)

#define virt_to_valid(x)  do {} while(0)

#define clear_page(addr) memset(phys_to_mem(__pa(addr)),0,PAGE_SIZE);

#endif
