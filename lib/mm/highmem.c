#include "../../include/linux/highmem.h"
#include "../../include/linux/list.h"
#include "../../include/linux/mm.h"
#include "../../include/linux/page-flags.h"
#include "../../include/linux/hash.h"
#include "../../include/linux/kernel.h"
#include "../../include/linux/smp.h"
#include "../../include/linux/fixmap.h"
#include "../../include/linux/tlbflush.h"

pte_t *pkmap_page_table;

static int pkmap_count[LAST_PKMAP];
int __kmp_atomic_idx;

#define PA_HASH_ORDER   7
/*
 * Describes on page->virtual association
 */
struct page_address_map {
	struct page *page;
	void *virtual;
	struct list_head list;
};
/*
 * page_address_map freelist,allocated from page_address_maps.
 */
static struct list_head page_address_pool;  /* freelist */
/*
 * Hash table bucket
 */
static struct page_address_slot {
	struct list_head lh;     /* List of page_address_maps */
//  spinlock_t lock;         /* Protect this bucket's list */
} page_address_htable[1 << PA_HASH_ORDER];

static struct page_address_slot *page_slot(struct page *page)
{
	return &page_address_htable[hash_ptr(page,PA_HASH_ORDER)];
}
/*
 * page-address - get the mapped virtual address of a page
 * Return the page's virtual address.
 */
void *page_address(struct page *page)
{
	unsigned long flags;
	void *ret;
	struct page_address_slot *pos;

	if(!PageHighMem(page))
		return lowmem_page_address(page);

	pos = page_slot(page);
	ret = NULL;
	//spin_lock_irqsave(&psa->lock,flags);
	if(!list_empty(&pos->lh))
	{
		struct page_address_map *pam;

		list_for_each_entry(pam,&pos->lh,list)
		{
			if(pam->page == page)
			{
				return pam->virtual;
				goto done;
			}
		}
	}
done:
	//spin_unlock_irqrestore(&pas->lock,flags);
	return ret;
}

void __kunmap_atomic(void *kvaddr)
{
	unsigned long vaddr = (unsigned long)kvaddr & PAGE_MASK;
	int idx,type;

	if(kvaddr >= (void *)FIXADDR_START)
	{
		type = kmap_atomic_idx();
		idx  = type + KM_TYPE_NR * smp_processor_id();

		if(cache_id_vivt())
//			__cpuc_flush_dcache_area((void *)vaddr,PAGE_SIZE);
#ifdef CONFIG_DEBUG_HIGHMEM
		BUG_ON(vaddr != __fix_to_virt(FIX_KMAP_DEGIN + idx));
		set_pte_ext(TOP_PTE(vaddr),__pte(0),0);
//		local_flush_tlb_kernel_page(vaddr);
#else
		(void)idx;  /* to kill a warning */
#endif
		kmap_atomic_idx_pop();
	} else if(vaddr >= PKMAP_ADDR(0) && vaddr < PKMAP_ADDR(LAST_PKMAP))
	{
		/* This address was obtained through kmap_high_get() */
		kunmap_high(pte_page(pkmap_page_table[PKMAP_NR(vaddr)]));
	}
	pagefault_enable();
}
static struct page_address_map page_address_maps[LAST_PKMAP];

void __init page_address_init(void)
{
	int i;

	INIT_LIST_HEAD(&page_address_pool);
	for(i = 0 ; i < ARRAY_SIZE(page_address_maps); i++)
		list_add(&page_address_maps[i].list,&page_address_pool);
	for(i = 0 ; i < ARRAY_SIZE(page_address_htable); i ++)
	{
		INIT_LIST_HEAD(&page_address_htable[i].lh);
//		spin_lock_init(&page_address_htable[i].lock);
	}
//  spin_lock_init(&pool_lock);
}
/*
 * pin a highmem page into memory.
 *
 * Return the page's current virtual memory address,or NULL if no mapping
 * exists.If and only if a non null address is returned then a matching 
 * call to kunmap_high() is necessary.
 *
 * This can be called from and context.
 */
void *kmap_high_get(struct page *page)
{
	unsigned long vaddr,flags;
	
//	lock_kmap_any(flags);
	vaddr = (unsigned long)page_address(page);
	if(vaddr)
	{
		BUG_ON(pkmap_count[PKMAP_NR(vaddr)] < 1);
		pkmap_count[PKMAP_NR(vaddr)]++;
	}
//	unlock_kmap_any(flags);
	return (void *)vaddr;
}
