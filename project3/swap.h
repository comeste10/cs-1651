/* Swap file implementation
 * (c) Jack Lange, 2012
 */

#include <linux/fs.h>


#ifndef __SWAP_H__
#define __SWAP_H__


struct swap_space {
	struct file * swap_file;
	u32 total_slots;
	// testing only remove this
	u64 victim_to_use;
};


struct swap_space * swap_init(u32 num_pages);
void swap_free(struct swap_space * swap);


int swap_out_page(struct swap_space * swap, u32 * index, void * page);
int swap_in_page(struct swap_space * swap, u32 index, void * dst_page);

#endif
