#include "../include/linux/debug.h"
#include "../include/linux/memblock.h"
#include "../include/linux/types.h"
#include "../include/linux/boot_arch.h"
#include "../include/linux/setup.h"

unsigned int high_to_low(unsigned int old)
{
	unsigned int hold = 0x00;
	int i;

	for(i = 0 ; i < 7 ; i++)
	{
		hold |= old & 0xF0000000;
		hold >>= 4;
		old <<= 4;
	}
	return hold | old;
}
/*
 * Memory debug tool
 */
/*
 * Show all region of Memory->type.
 */
void R_show(struct memblock_type *type,char *s)
{
    int i;

    for(i = type->cnt - 1 ; i >= 0 ; i--)
    {
        mm_debug("Region[%d][%p - %p][%s]\n",i,
                (void *)type->regions[i].base,
                (void *)(type->regions[i].base +
                    type->regions[i].size),s);
    }
}
/*
 * Show some memory segment of Memory_array.
 */
//void M_show(unsigned int start,unsigned int end)
void M_show(phys_addr_t start,phys_addr_t end)
{
    int i,j;

	unsigned int *memory_array;
#ifndef CONFIG_BOTH_BANKS
	memory_array = memory_array0;
#else
	if(start >= CONFIG_BANK1_START )
		memory_array = memory_array1;
	else
		memory_array = memory_array0;
#endif

	start -= PHYS_OFFSET;
	end   -= PHYS_OFFSET;

    printf("=============Memory=============\n");
//  for(i = 0 ; i < SZ_256M / BYTE_MODIFY / 5 ; i++)
    for(i = start / BYTE_MODIFY / 5; i <= end / BYTE_MODIFY / 5 ; i++)
    {
        printf("\n[%08x]",(unsigned int)(i * 5 * BYTE_MODIFY + PHYS_OFFSET));
        for(j = 0 ; j < 5 ; j++)
            printf("\t%08x",high_to_low(memory_array[i * 5 + j]));
    }
    mm_debug("\n=====================================\n");
}
/*
 * Show all bitmap of all memory.
 */
void B_show(unsigned int *bitmap)
{
    phys_addr_t start_pfn,end_pfn;
    unsigned long i;
    unsigned long bytes;
    unsigned char *byte_char = (unsigned char *)bitmap;
#if 0
    start_pfn = phys_to_pfn(memory.start);
    end_pfn   = phys_to_pfn(memory.end);
    bytes = (end_pfn - start_pfn + 7) / 8;
    mm_debug("=============== BitMap =============\n");
    for(i = 0 ; i < 20 ; i++)
    {
        unsigned char byte = byte_char[i];
        int j;

        mm_debug("\nByte[%ld] ",i);
        for(j = 0 ; j < 8 ; j++)
        {
            unsigned char bit = byte & 0x01;

            mm_debug("\t%ld ",(unsigned long)bit);
            byte >>= 1;
        }

    }
    mm_debug("\n||");
    for(i = bytes - 20 ; i < bytes ; i++)
    {
        unsigned char byte = byte_char[i];
        int j;

        mm_debug("\nByte[%ld] ",i);
        for(j = 0 ; j < 8 ; j++)
        {
            unsigned char bit = byte & 0x01;

            mm_debug("\t%ld ",(unsigned long)bit);
            byte >>= 1;
        }
    }
    mm_debug("\n======================================\n");
#endif
}
/*
 * Show membank
 */
void BK_show(char *s)
{
	int i;

	mm_debug("=================MemoryBank======================\n");
	for(i = 0 ; i < meminfo.nr_banks ; i++)
	{
		mm_debug("BANK[%d]\tregion[%08x - %08x]\tsize[%08x][%s]\n",
				i,meminfo.bank[i].start,
				meminfo.bank[i].start + meminfo.bank[i].size,
				meminfo.bank[i].size,s);
	}
	mm_debug("=================================================\n");
}
