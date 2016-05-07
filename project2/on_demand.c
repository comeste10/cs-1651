/* On demand Paging Implementation
 * (c) Jack Lange, 2012
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/sched.h>

#include "petmem.h"
#include "on_demand.h"
#include "pgtables.h"
#include "swap.h"

// LIST OPERATIONS

int insert_new_node(u64 sa, u64 s, struct list_head * head) {
	struct vaddr_reg * newNode = (struct vaddr_reg *)kmalloc(sizeof(struct vaddr_reg),GFP_KERNEL);
	newNode->start_address = sa;
	newNode->size = s;
	if(head == NULL) {
		return -1;
	}
	if(list_empty(head)) {
		list_add(&(newNode->list_member), head);
		return 0;
	}
	struct vaddr_reg * tempNode;
	struct vaddr_reg * prevNode;
	struct list_head * iterator;
	__list_for_each(iterator, head) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		if(tempNode->start_address > sa) {
			if(iterator->prev == head) {
				list_add(&(newNode->list_member), head);
				return 0;
			}
			prevNode = list_entry(iterator->prev, struct vaddr_reg, list_member);
			// if no overlap			
			if((prevNode->start_address + prevNode->size) <= sa) {
				list_add(&(newNode->list_member), iterator->prev);
				return 0;
			}
			// if overlap
			else {
				return -1;
			}
		}
	}
	// insert at end of list
	list_add(&(newNode->list_member), iterator->prev);
	return 0;
}

int insert_node(struct vaddr_reg * node, struct list_head * head) {
	if(head == NULL) {
		return -1;
	}
	if(list_empty(head)) {
		list_add(&(node->list_member), head);
		return 0;
	}
	struct vaddr_reg * tempNode;
	struct vaddr_reg * prevNode;
	struct list_head * iterator;
	__list_for_each(iterator, head) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		if(tempNode->start_address > node->start_address) {
			if(iterator->prev == head) {
				list_add(&(node->list_member), head);
				return 0;
			}
			prevNode = list_entry(iterator->prev, struct vaddr_reg, list_member);
			// if no overlap			
			if((prevNode->start_address + prevNode->size) <= node->start_address) {
				list_add(&(node->list_member), iterator->prev);
				return 0;
			}
			// if overlap
			else {
				return -1;
			}
		}
	}
	// insert at end of list
	list_add(&(node->list_member), iterator->prev);
	return 0;
}

void remove_node(struct list_head * node) {
	list_del(node);
	//free(node);
}

struct vaddr_reg * first_fit(u64 request_size, struct list_head * head) {
	struct list_head * iterator;
	struct vaddr_reg * tempNode;
	__list_for_each(iterator, head) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		if(tempNode->size >= request_size) break;
	}
	if(iterator == head) {
		printk("request failed\n");
		return NULL;
	}
	else {
		remove_node(iterator);
		u64 remaining_size = tempNode->size - request_size;
		u64 remaining_start = tempNode->start_address + request_size;
		if(remaining_size) {
			insert_new_node(remaining_start, remaining_size, head);
		}
		tempNode->size = request_size;
		return tempNode;
	}
}

void coalesce_list(struct list_head * head) {
	if(list_empty(head)) {
		return;
	}
	struct list_head * iterator;
	struct vaddr_reg * prevNode;
	struct vaddr_reg * currNode;
	u64 diff;
	__list_for_each(iterator, head) {
		if(iterator == head || iterator->prev == head) continue;
		prevNode = list_entry(iterator->prev, struct vaddr_reg, list_member);
		currNode = list_entry(iterator, struct vaddr_reg, list_member);
		diff = currNode->start_address - (prevNode->start_address + prevNode->size);
		if(!diff) {
			//printk("found adjacent free blocks\n");
			prevNode->size += currNode->size;
			iterator = &(prevNode->list_member);
			remove_node(&(currNode->list_member));
			kfree(currNode);
		}
	}
}

int myfree(uintptr_t addr, struct list_head * usedHead, struct list_head * freeHead) {

	printk("in myfree\n");
	struct list_head * iterator;
	struct vaddr_reg * tempNode;
	int badAddress = 1;
	__list_for_each(iterator, usedHead) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		if(tempNode->start_address == (u64)(addr)) {
			badAddress = 0;
			break;
		}
	}
	if(badAddress) {
		printk("myfree failed\n");
		return -1;
	}
	remove_node(iterator); // same as tempNode->list_member ** don't free this
	insert_node(tempNode, freeHead);
	coalesce_list(freeHead);
	printk("end of myfree\n");
	return 0;
}

void print_vaddr_reg_list(struct list_head * head) {
	if(list_empty(head)) {
		printk("EMPTY LIST\n\n");
		return;
	}
	struct list_head * iterator;
	struct vaddr_reg * tempNode;
	printk(" start_addr \tsize\n");
	printk("------------\t----\n");
	__list_for_each(iterator, head) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		printk("0x%-10llX\t%llu\n", tempNode->start_address, tempNode->size);
		//printk("%-12lld\t%llu\n", tempNode->start_address, tempNode->size);
	}
	printk("\n");
}

// END OF LIST OPERATIONS


struct mem_map * petmem_init_process() {
	struct mem_map * map = (struct mem_map *)kmalloc(sizeof(struct mem_map),GFP_KERNEL);
	INIT_LIST_HEAD(&(map->usedListHead));
	INIT_LIST_HEAD(&(map->freeListHead));
	u64 initial_size = PETMEM_REGION_END-PETMEM_REGION_START;
	insert_new_node(PETMEM_REGION_START, initial_size, &(map->freeListHead));
	return map;
}


void petmem_deinit_process(struct mem_map * map) {
	printk("in petmem_deinit_process\n");
	struct list_head * iterator;
	struct vaddr_reg * nextNode;
	
	// kfree each node in usedList
	iterator = &(map->usedListHead);
	while(iterator->next != &(map->usedListHead)) {
		nextNode = list_entry(iterator->next, struct vaddr_reg, list_member);
		kfree(nextNode);
		iterator = iterator->next;
	}

	// kfree each node in freeList
	iterator = &(map->freeListHead);
	while(iterator->next != &(map->freeListHead)) {
		nextNode = list_entry(iterator->next, struct vaddr_reg, list_member);
		kfree(nextNode);
		iterator = iterator->next;
	}
	
	kfree(map);
}


uintptr_t petmem_alloc_vspace(struct mem_map * map, u64 num_pages) { 
	printk("Memory allocation\n");
	u64 num_bytes = 4096*num_pages;
	struct vaddr_reg * req = (struct vaddr_reg *)first_fit(num_bytes, &(map->freeListHead));
	insert_node(req, &(map->usedListHead));
	return (uintptr_t)req->start_address;
}


void petmem_dump_vspace(struct mem_map * map) {
	printk("\n");
	printk("USED LIST\n");
	print_vaddr_reg_list(&(map->usedListHead));
	printk("FREE LIST\n");
	print_vaddr_reg_list(&(map->freeListHead));
    return;
}

int table_not_used(uintptr_t table) {
	int i;
	int present = 0;
	for(i=0; i<512; i++) {
		table += 8*i;
		present = table & 0x1;
		if(present) {
			return 0;
		}
	}
	return 1;
}

void petmem_free_vspace(struct mem_map * map, uintptr_t vaddr) {
	printk("Free memory\n");
	struct list_head * iterator;
	struct vaddr_reg * tempNode;

	//petmem_dump_vspace(map);

	int badAddress = 1;
	__list_for_each(iterator, &(map->usedListHead)) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		//printk("tempNode->start_address: %llX\n", tempNode->start_address);
		//printk("vaddr: %llX\n", tempNode->start_address);
		if(tempNode->start_address == (u64)(vaddr)) {
			badAddress = 0;
			break;
		}
	}
	if(badAddress) {
		printk("failed in petmem_free_vspace, bad address\n");
		//myfree((u64)vaddr, &(map->usedListHead), &(map->freeListHead));
		return;
	}

	u64 page_count = (int)(tempNode->size/4096);
	if(tempNode->size % 4096 != 0) page_count++;

	printk("page count: %llu\n", page_count);
	// check page count	
	//return;

	pml4e64_t * pml4e;
	pdpe64_t * pdpe;
	pde64_t * pde;
	pte64_t * pte;
	uintptr_t page;

	uintptr_t free_vaddr = vaddr;
	uintptr_t tempBase;
	uintptr_t tempOff;
	int i;
	for(i=0; i<(int)page_count; i++) {
	
		// get PML4E
		tempBase = CR3_TO_PML4E64_VA(get_cr3());
		tempOff = PML4E64_INDEX(free_vaddr);
		pml4e = tempBase + (tempOff << 3);

		printk("gets PML4E\n");
		
		// get PDPE
		tempBase = BASE_TO_PAGE_ADDR(pml4e->pdp_base_addr);
		tempOff = PDPE64_INDEX(free_vaddr);
		pdpe = __va(tempBase + (tempOff << 3));

		printk("gets PDPE\n");

		// get PDE
		tempBase = BASE_TO_PAGE_ADDR(pdpe->pd_base_addr);
		tempOff = PDE64_INDEX(free_vaddr);
		pde = __va(tempBase + (tempOff << 3));
	
		printk("gets PDE\n");

		// get PTE
		tempBase = BASE_TO_PAGE_ADDR(pde->pt_base_addr);
		tempOff = PTE64_INDEX(free_vaddr);
		pte = __va(tempBase + (tempOff << 3));

		printk("gets PTE\n");

		// get page
		tempBase = BASE_TO_PAGE_ADDR(pte->page_base_addr);
		tempOff = free_vaddr & 0xFFF;
		page = __va(tempBase + tempOff);

		printk("gets page\n");

		// invalidate TLB, free physical page
		invlpg(page); 
		petmem_free_pages(page, 1);

		printk("invalidates TLB, frees page\n");

		// handle PTE
		pte->present = 0;

		printk("handles PTE\n");

		// check if PT can be freed
		tempBase = __va(BASE_TO_PAGE_ADDR(pde->pt_base_addr));
		if(table_not_used(tempBase)) {
			pde->present = 0;
			petmem_free_pages(__pa(tempBase),1);

			printk("frees PT\n");

			// check if PD can be freed
			tempBase = __va(BASE_TO_PAGE_ADDR(pdpe->pd_base_addr));
			if(table_not_used(tempBase)) {
				pdpe->present = 0;
				petmem_free_pages(__pa(tempBase),1);

				printk("frees PD\n");

				// check if PDP can be freed
				tempBase = __va(BASE_TO_PAGE_ADDR(pml4e->pdp_base_addr));
				if(table_not_used(tempBase)) {
					pml4e->present = 0;
					petmem_free_pages(__pa(tempBase),1);

					printk("frees PDP\n");
				}
			}
		}

		// increment freeing address to deal with remaining pages
		free_vaddr += 4096;
	}

	// free virtual address space for user process
	myfree(vaddr, &(map->usedListHead), &(map->freeListHead));
	printk("reaches end of petmem_free_vspace\n");
    return;
}


/* 
   error_code:
       1 == not present
       2 == permissions error
*/

int petmem_handle_pagefault(struct mem_map * map, uintptr_t fault_addr, u32 error_code) {
    printk("Page fault!\n");

	// check error type
	if(error_code != 1) {
		printk("Page fault dropped, wrong error code\n");
		return -1;
	}

	// check if map owns fault_addr
	int badAddress = 1;
	struct list_head * iterator;
	struct vaddr_reg * tempNode;
	uintptr_t testBase;
	uintptr_t testBound;
	__list_for_each(iterator, &(map->usedListHead)) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		testBase = tempNode->start_address;
		testBound = testBase + tempNode->size;
		if(fault_addr >= testBase && fault_addr <= testBound) {
			badAddress = 0;
			break;
		}
	}
	if(badAddress) {
		printk("Page fault dropped, bad address\n");
		//printk("leaving petmem_handle_pagefault\n");
		return -1;
	}
	
	pml4e64_t * pml4e;
	pdpe64_t * pdpe;
	pde64_t * pde;
	pte64_t * pte;

	uintptr_t tempBase = 0;
	uintptr_t tempOff = 0;

	// get page from buddy allocator
	uintptr_t page_addr = petmem_alloc_pages(1);

	// check for failed buddy alloc
	if(page_addr == NULL) {
		printk("Page fault dropped, buddy alloc failed\n");
		//printk("leaving petmem_handle_pagefault\n");		
		return -1;
	}

	// handle PML4E
	tempBase = CR3_TO_PML4E64_VA(get_cr3());
	tempOff = PML4E64_INDEX(fault_addr);
	pml4e = tempBase + (tempOff << 3);
	if(pml4e->present == 0) {
		pml4e->pdp_base_addr = PAGE_TO_BASE_ADDR(petmem_alloc_pages(1));
		pml4e->present = 1;
	}
	pml4e->writable = 1;
	pml4e->user_page = 1;

	// handle PDPE
	tempBase = BASE_TO_PAGE_ADDR(pml4e->pdp_base_addr);
	tempOff = PDPE64_INDEX(fault_addr);
	pdpe = __va(tempBase + (tempOff << 3));
	if(pdpe->present == 0) {
		pdpe->pd_base_addr = PAGE_TO_BASE_ADDR(petmem_alloc_pages(1));
		pdpe->present = 1;
	}
	pdpe->writable = 1;
	pdpe->user_page = 1;

	// handle PDE
	tempBase = BASE_TO_PAGE_ADDR(pdpe->pd_base_addr);
	tempOff = PDE64_INDEX(fault_addr);
	pde = __va(tempBase + (tempOff << 3));
	if(pde->present == 0) {
		pde->pt_base_addr = PAGE_TO_BASE_ADDR(petmem_alloc_pages(1));
		pde->present = 1;
	}
	pde->writable = 1;
	pde->user_page = 1;

	// handle PTE
	tempBase = BASE_TO_PAGE_ADDR(pde->pt_base_addr);
	tempOff = PTE64_INDEX(fault_addr);
	pte = __va(tempBase + (tempOff << 3));
	pte->present = 1;
	pte->writable = 1;
	pte->user_page = 1;
	pte->page_base_addr = PAGE_TO_BASE_ADDR(page_addr);

    return 0;
}













