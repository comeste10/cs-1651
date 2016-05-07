/* Swap file implementation
 * (c) Jack Lange, 2012
 */


#include <linux/slab.h>


#include "file_io.h"
#include "swap.h"


struct swap_space * swap_init(u32 num_pages) {
	struct swap_space * swap = (struct swap_space *)kmalloc(sizeof(struct swap_space),GFP_KERNEL);
	swap->swap_file = file_open("/tmp/cs1651.swap",O_RDWR);
	if(swap->swap_file == NULL) {
		printk("failed to open /tmp/cs1651.swap\n");
		return NULL;
	}	
	unsigned long long swap_file_bytes = file_size(swap->swap_file);
	swap->total_slots = (u32)(swap_file_bytes/4096);
	printk("total page slots: %llu\n", (u64)swap->total_slots);

	// testing only, remove this
	swap->victim_to_use = 0;

	return swap;
}


void swap_free(struct swap_space * swap) {
	file_close(swap->swap_file);
}


// Swap out
//		-no free pages left in memory
//		-"page" was chosen for eviction
//		1. find the index of a free page slot in the swap area
//		2. write "page" to this slot
//		3. set index to index that was used
//		4. clear "page"
int swap_out_page(struct swap_space * swap, u32 * index, void * page) {
	printk("in swap_out_page\n");
	if(page == NULL) {
		printk("error: attempting to swap out null page\n");
		return -1;
	}

	// find empty page slot
	// scan through file until slot is empty	
	char * buf = kmalloc(4096, GFP_KERNEL);
	u64 victim_byte_offset;
	int noSlotFound = 1;
	u32 psi;
	for(psi=0; psi<(swap->total_slots); psi++) {
		victim_byte_offset = 4096 * psi;
		memset(buf, 0, 4096);
		file_read(swap->swap_file, buf, 4096, victim_byte_offset);
		int i;
		char testByte = 0;
		for(i=0; i<4096; i++) {
			testByte |= buf[i];
		}
		if(testByte == 0) {
			printk("found empty page slot at index %llu\n", (u64)psi);
			noSlotFound = 0;
			break;
		}
	}
	kfree(buf);
	if(noSlotFound) {
		printk("no empty page slots\n");
		return -1;
	}
	
	// set index to empty slot
	*index = psi;
	printk("page: %p\n", (void *)page);

	// write "page" to slot at "*index"
	file_write(swap->swap_file, __va(page), 4096, (*index)*4096);

	printk("leaving swap_out_page\n");	
    return 0;
}


// Swap in
//		-process accessed swapped out page
//		-"dst_page" was previously swapped out
//		1. retrieve page at page_slots[index]
//		2. store contents in dst_page
int swap_in_page(struct swap_space * swap, u32 index, void * dst_page) {
	printk("in swap_in_page\n");
	printk("page slot index: %llu\n", (u64)index);
	if(index >= (swap->total_slots)) {
		printk("index out of bounds\n");
		return -1;
	}
	
	// read contents at index into destination page
	file_read(swap->swap_file, __va(dst_page), 4096, index*4096);
	printk("\n%c\n\n", ((char *)__va(dst_page))[0]);  // should be 'a' per my test program
	
	// clear page slot
	char * buf = kmalloc(4096, GFP_KERNEL);
	memset(buf, 0, 4096);
	file_write(swap->swap_file, buf, 4096, index*4096);
	kfree(buf);
	
	printk("leaving swap_in_page\n");
    return 0;
}



