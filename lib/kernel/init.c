#include "../../include/linux/kernel.h"
#include "../../include/linux/setup.h"
#include "../../include/linux/mm_types.h"
#include "../../include/linux/mman.h"
#include "../../include/linux/memory.h"
#include "../../include/linux/pgtable.h"
#include "../../include/linux/highmem.h"
#include "../../include/linux/vmalloc.h"
#include "../../include/linux/memblock.h"
#include "../../include/linux/mmzone.h"
#include "../../include/linux/mm.h"
#include "../../include/linux/swap.h"
#include "../../include/linux/gfp.h"
#include "../../include/linux/page-flags.h"
#include "../../include/linux/vmstat.h"
#include "../../include/linux/init.h"

extern unsigned long max_pfn;
extern unsigned long min_low_pfn;
extern unsigned long max_low_pfn;

static unsigned long phys_initrd_start __initdata = 0;
static unsigned long phys_initrd_size  __initdata = 0;

static int __init early_initrd(char *p)
{
	unsigned long start,size;
	char *endp;

	start = memparse(p,&endp);
	if(*endp == ',') {
		size = memparse(endp + 1 , NULL);

		phys_initrd_start = start;
		phys_initrd_size  = size;
	}
	return 0;
}
early_param("initrd",early_initrd);

/*
 * This keeps memory configuration data used by a couple memory
 * initialization functions,as well as show_mem() for keep the skipping
 * of holes in the memory map.It is populated by arm_add_memory().
 */
struct meminfo meminfo;
static int __init meminfo_cmp(const void *_a,const void *_b)
{
	const struct membank *a = _a,*b = _b;
	long cmp = bank_pfn_start(a) - bank_pfn_start(b);
	return cmp < 0 ? -1 : cmp > 0 ? 1 : 0;
}
/*
 * Initialize memblock of ARM
 */
void __init arm_memblock_init(struct meminfo *mi)
{
	int i;

	sort(&meminfo.bank,meminfo.nr_banks,
			sizeof(meminfo.bank[0]),meminfo_cmp,NULL);

	memblock_init();
	for(i = 0 ; i < mi->nr_banks ; i++)
		memblock_add(mi->bank[i].start,mi->bank[i].size);
	/*
	 * Add reserved region for swapper_pg_dir.
	 */
	arm_mm_memblock_reserve();
	/*
	 * calculate the total size of memblock.memory.regions
	 */
	memblock_analyze();
	/*
	 * Dump all information of memory and reserved.
	 */
	memblock_dump_all();
}
/*
 * Initilize the early parment.
 */
void early_parment(void)
{
	/*
	 * Initilize the VMALLOC_AREA.
	 */
}
/*
 * Find the normal memory limit.
 */
void __init find_limits(unsigned int *min,unsigned int *max_low,
		unsigned int *max_high)
{
	struct meminfo *mi = &meminfo;
	int i;
	
	*min = 0xFFFFFFFF;
	*max_low = *max_high = 0;

	for_each_bank(i,mi) {
		struct membank *bank = &mi->bank[i];
		unsigned int start,end;

		start = bank_pfn_start(bank);
		end   = bank_pfn_end(bank);

		if(*min > start)
			*min = start;
		if(*max_high < end)
			*max_high = end;
		if(bank->highmem)
			continue;
		if(*max_low < end)
			*max_low = end;
	}
}
static void arm_memory_present(void)
{
}
/*
 * Bootmem init
 */
void __init bootmem_init(void)
{
	unsigned int min,max_low,max_high;

	max_low = max_high = 0;

	find_limits(&min,&max_low,&max_high);
	
	arm_bootmem_init(min,max_low);

	/*
	 * Sparsemem tries to allocate bootmem in memory_present().
	 * so must be done after the fixed reservations.
	 */
	arm_memory_present();

	/*
	 * sparse_init() needs the bootmem allocator up and running.
	 */
	sparse_init();
	/*
	 * Now free the memory - free_area_init_node needs
	 * that sparse mem_map arrays initalized by sparse_init()
	 * for memmap_init_zone(),otherwise all PFNs are invalid.
	 */
	arm_bootmem_free(min,max_low,max_high);
	
	high_memory = (void *)__va(((max_low) << PAGE_SHIFT) - 1) + 1;
	
	/*
	 * This doesn't seem to be used by the Linux memory manager any
	 * more,but is used by ll_rw_block.If we can get rid of it,we
	 * also get rid of some of the stuff above as well.
	 *
	 * Note:max_low_pfn and max_pfn reflect the number of _pages_ in
	 * the system,not the maximum PFN.
	 */
	max_low_pfn = max_low - PHYS_PFN_OFFSET;
	max_pfn     = max_high - PHYS_PFN_OFFSET;
}
/*
 * Free memmap.
 */
static inline void free_memmap(unsigned long start_pfn,unsigned long end_pfn)
{
	struct page *start_pg,*end_pg;
	unsigned long pg,pgend;

	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page(start_pfn - 1) + 1;
	end_pg   = pfn_to_page(end_pfn);

	/*
	 * Convert to physical addresses,and
	 * round start upwards and end downwards.
	 */
	pg = PAGE_ALIGN(__pa(start_pg));
	pgend = __pa(end_pg) & PAGE_MASK;

	/*
	 * If there are free pages between these,
	 * free the section of the memmap array.
	 */
	if(pg < pgend)
		free_bootmem(pg,pgend - pg);
}
/*
 * The mem_map array can get very big.Free the unused area of the memory map.
 */
static void __init free_unused_memmap(struct meminfo *mi)
{
	unsigned long bank_start,prev_bank_end = 0;
	unsigned int i;

	/*
	 * This relies on each bank being in address order.
	 * The banks are sorted previously in bootmem_init().
	 */
	for_each_bank(i,mi)
	{
		struct membank *bank = &mi->bank[i];

		bank_start = bank_pfn_start(bank);

		/*
		 * If we had a previous bank,and there is a space
		 * between the current bank and the previous.free it.
		 */
		if(prev_bank_end && prev_bank_end < bank_start)
			free_memmap(prev_bank_end,bank_start);

		/*
		 * Align up here since the VM subsystem insists that the 
		 * memmap entries are valid from the bank end aligned to 
		 * MAX_ORDER_NR_PAGES.
		 */
		prev_bank_end = ALIGN(bank_pfn_end(bank),MAX_ORDER_NR_PAGES);
	}
}
static inline int free_area(unsigned long pfn,unsigned long end,char *s)
{
	unsigned int pages = 0,size = (end - pfn) << (PAGE_SHIFT - 10);

	for(; pfn < end ; pfn++)
	{
		struct page *page = pfn_to_page(pfn);
		
		ClearPageReserved(page);
		init_page_count(page);
		__free_page(page);
		page++;
	}
	if(size && s)
		mm_debug("Freeing %s memory %dK\n",s,size);
	
	return pages;
}
/*
 * Free highpage in initialition.
 */
static void __init free_highpages(void)
{
#ifdef CONFIG_HIGHMEM
	unsigned long max_low = max_low_pfn + PHYS_PFN_OFFSET;
	struct memblock_region *mem,*res;

	/* Set highmem page free */
	for_each_memblock(memory,mem)
	{
		unsigned long start = memblock_region_memory_base_pfn(mem);
		unsigned long end   = memblock_region_memory_end_pfn(mem);

		/* Ignore complete lowmem entries */
		if(end <= max_low)
			continue;

		/* Truncate partial highmem entries */
		if(start < max_low)
			start = max_low;

		/* Find and exclude any reserved regions */
		for_each_memblock(reserved,res)
		{
			unsigned long res_start,res_end;

			res_start = memblock_region_reserved_base_pfn(res);
			res_end   = memblock_region_reserved_end_pfn(res);

			if(res_end < start)
				continue;
			if(res_start < start)
				res_start = start;
			if(res_start > end)
				res_start = end;
			if(res_end > end)
				res_end = end;
			if(res_start != start)
				totalhigh_pages += free_area(start,res_start,NULL);

			start = res_end;
			if(start == end)
				break;
		}
		/*
		 * And now free anything which remains.
		 */
		if(start < end)
			totalhigh_pages += free_area(start,end,NULL);
	}
	totalram_pages += totalhigh_pages;
#endif
}
/*
 * mem_init() marks the free areas in the mem_map and tells us how much
 * memory is free.This is done after various parts of the system have
 * claimed their memory after the kernel image.
 */
void __init mem_init(void)
{
	unsigned long reserved_pages,free_pages;
	struct memblock_region *reg;
	int i;

	max_mapnr = (struct page *)(unsigned long)mem_to_phys(pfn_to_page(max_pfn 
				+ PHYS_PFN_OFFSET)) - mem_map;

	/* This will put all unused low memory onto the freelists. */
	free_unused_memmap(&meminfo);

	totalram_pages += free_all_bootmem();
#ifdef CONFIG_SA1111
	/* Intel StrongARM not support */
	total_pages += free_area(PHYS_PFN_OFFSET,
			__phys_to_pfn(__pa(swapper_pg_dir)),NULL);
#endif
	free_highpages();

	reserved_pages = free_pages = 0;

	for_each_bank(i,&meminfo)
	{
		struct membank *bank = &meminfo.bank[i];
		unsigned int pfn1,pfn2;
		struct page *page,*end;

		pfn1 = bank_pfn_start(bank);
		pfn2 = bank_pfn_end(bank);

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		do {
			//if(PageReserved(page))
			if(0)
				reserved_pages++;
			else if(!page_count(page))
				free_pages++;
		} while(page < end);
	}

	/*
	 * Since our memory may not be contiguous,calculate the 
	 * real number of pages we have in this system.
	 */
	mm_debug("Memory:");
	num_physpages = 0;
	for_each_memblock(memory,reg)
	{
		unsigned long pages = memblock_region_memory_end_pfn(reg) - 
			memblock_region_memory_base_pfn(reg);
		num_physpages += pages;
		mm_debug("%ldMB",pages >> (20 - PAGE_SHIFT));
	}
	mm_debug(" = %luMB total\n",num_physpages >> (20 - PAGE_SHIFT));

	mm_debug("Memory: %luK/ %luK available,%luk reserved,%luK highmem\n",
			nr_free_pages() << (PAGE_SHIFT - 10),
			free_pages << (PAGE_SHIFT - 10),
			reserved_pages << (PAGE_SHIFT - 10),
			totalhigh_pages << (PAGE_SHIFT - 10));

#define MLK(b,t) (void *)b,(void *)t,(void *)((t) - (b) >> 10)
#define MLM(b,t) (void *)b,(void *)t,(void *)((t) - (b) >> 20)
#define MLK_ROUNDUP(b,t) (void *)b,(void *)t,(void *)DIV_ROUND_UP(((t) - (b)),SZ_1K)
	mm_debug("Vitual kernel memory layout:\n"
			"   vector : 0x%p - 0x%p (%p kB)\n"
			"   fixmap : 0x%p - 0x%p (%p kB)\n"
#ifdef CONFIG_MMU
			"   DMA    : 0x%p - 0x%p (%p MB)\n"
#endif
			"   vmalloc: 0x%p - 0x%p (%p MB)\n"
			"   lowmem : 0x%p - 0x%p (%p MB)\n"
#ifdef CONFIG_HIGHMEM
			"   pkmap  : 0x%p - 0x%p (%p MB)\n"
#endif
			"   modules: 0x%p - 08%p (%p MB)\n"
			"   .init  : 0x%p - 0x%p (%p kB)\n"
			"   .text  : 0x%p - 0x%p (%p kB)\n"
			"   .data  : 0x%p - 0x%p (%p kB)\n",
			MLK(UL(CONFIG_VECTORS_BASE),UL(CONFIG_VECTORS_BASE) +
				(PAGE_SIZE)),
			MLK(FIXADDR_START,FIXADDR_TOP),
#ifdef CONFIG_MMU
			MLM(CONSISTENT_BASE,CONSISTENT_END),
#endif
			MLM(VMALLOC_START,VMALLOC_END),
			MLM(PAGE_OFFSET,(unsigned long)high_memory),
#ifdef CONFIG_HIGHMEM
			MLM(PKMAP_BASE,(PKMAP_BASE) + (LAST_PKMAP) *
					(PAGE_SIZE)),
#endif
			MLM(MODULES_VADDR,MODULES_END),

			MLK_ROUNDUP(0,0),
			MLK_ROUNDUP(0,0),
			MLK_ROUNDUP(0,0));
#undef MLK
#undef MLM
#undef MLK_ROUNDUP

	/*
	 * Check boundaries twice:Some fundamental inconsistencies can
	 * be detected at build time already.
	 */
#ifdef CONFIG_MMU
	BUILD_BUG_ON(VMALLOC_END      > CONSISTENT_BASE);
	BUILD_ON(VMALLOC_END          > CONSISTENT_BASE);

	BUILD_BUG_ON(TASK_SIZE        > MODULES_VADDR);
	BUG_ON(TASK_SIZE              > MODULES_VADDR);
#endif

#ifdef CONFIG_HIGHMEM
	BUILD_BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE > PAGE_OFFSET);
	BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE  > PAGE_OFFSET);
#endif

	if(PAGE_SIZE >= 16384 && num_physpages <= 128)
	{
		extern int sysctl_overcommit_memory;

		/*
		 * On a machine this small we won't get 
		 * anywhere without overcommit,so turn
		 * it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}

/*
 * show memory.
 */
void show_mem(void)
{
	int free = 0,total = 0,reserved = 0;
	int shared = 0,cached = 0,slab = 0,i;
	struct meminfo *mi = &meminfo;

	mm_debug("Mem-info:\n");
	show_free_areas();

	for_each_bank(i,mi)
	{
		struct membank *bank = &mi->bank[i];
		unsigned int pfn1,pfn2;
		struct page *page,*end;

		pfn1 = bank_pfn_start(bank);
		pfn2 = bank_pfn_end(bank);

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		do {
			total++;
			//if(PageReserved(page))
			if(0)
				reserved++;
			//else if(PageSwapCache(page))
			else if(0)
				cached++;
			//else if(PageSlab(page))
			else if(0)
				free++;
			else
				shared += page_count(page) - 1;
			page++;
		} while(page < end);
	}
	mm_debug("%p pages of RAM\n",(void *)(unsigned long)total);
	mm_debug("%p free pages\n",(void *)(unsigned long)free);
	mm_debug("%p reserved pages\n",(void *)(unsigned long)reserved);
	mm_debug("%p slab pages\n",(void *)(unsigned long)slab);
	mm_debug("%p pages shared\n",(void *)(unsigned long)shared);
	mm_debug("%p pages swap cached\n",(void *)(unsigned long)cached);
}











