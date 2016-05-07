/* On demand Paging Implementation
 * (c) Jack Lange, 2012
 */

#ifndef __ON_DEMAND_H__
#define __ON_DEMAND_H__

#include <linux/module.h>

#include "swap.h"

//#include "list.h"
//typedef unsigned long u64;

struct vaddr_reg {
	u64 start_address;
	u64 size;
	struct list_head list_member;
};

struct mem_map {
	// Virtual address space mapping state
	// Range: [PETMEM_REGION_START, PETMEM_REGION_END]
	struct list_head usedListHead;
	struct list_head freeListHead;
};

// LIST OPERATIONS

//int insert_new_node(u64 sa, u64 s, struct list_head * head);

//int insert_node(struct vaddr_reg * node, struct list_head * head);

//void remove_node(struct list_head * node);

//struct vaddr_reg * first_fit(u64 request_size, struct list_head * head);

//int myfree(u64 addr, struct list_head * usedHead, struct list_head * freeHead);

//void print_vaddr_reg_list(struct list_head * head);

// END OF LIST OPERATIONS


struct mem_map * petmem_init_process(void);
void petmem_deinit_process(struct mem_map * map);

uintptr_t petmem_alloc_vspace(struct mem_map * map, u64 num_pages);
void petmem_free_vspace(struct mem_map * map, uintptr_t vaddr);

void petmem_dump_vspace(struct mem_map * map);

// How do we get error codes??
int petmem_handle_pagefault(struct mem_map * map, uintptr_t fault_addr, u32 error_code);

#endif
