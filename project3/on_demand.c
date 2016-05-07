/* On demand Paging Implementation
 * (c) Jack Lange, 2012
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/random.h>

#include "petmem.h"
#include "on_demand.h"
#include "pgtables.h"
#include "swap.h"

///////////////// MY FUNCTIONS /////////////////

int insert_new_node(u64 sa, u64 s, struct list_head * head) {
	if(head == NULL) return -1;
	struct vaddr_reg * newNode = (struct vaddr_reg *)kmalloc(sizeof(struct vaddr_reg),GFP_KERNEL);
	newNode->start_address = sa;
	newNode->size = s;
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
			if((prevNode->start_address + prevNode->size) <= sa) {
				list_add(&(newNode->list_member), iterator->prev);
				return 0;
			}
			else {
				return -1;
			}
		}
	}
	list_add(&(newNode->list_member), iterator->prev);
	return 0;
}

int insert_node(struct vaddr_reg * node, struct list_head * head) {
	if(head == NULL) return -1;
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
			if((prevNode->start_address + prevNode->size) <= node->start_address) {
				list_add(&(node->list_member), iterator->prev);
				return 0;
			}
			else {
				return -1;
			}
		}
	}
	list_add(&(node->list_member), iterator->prev);
	return 0;
}

void remove_node(struct list_head * node) {
	list_del(node);
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
	if(list_empty(head)) return;
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
			prevNode->size += currNode->size;
			iterator = &(prevNode->list_member);
			remove_node(&(currNode->list_member));
			kfree(currNode);
		}
	}
}

int myfree(uintptr_t addr, struct list_head * usedHead, struct list_head * freeHead) {
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
	remove_node(iterator);
	insert_node(tempNode, freeHead);
	coalesce_list(freeHead);
	return 0;
}

void print_vaddr_reg_list(struct list_head * head) {
	if(list_empty(head)) {
		printk("empty list\n\n");
		return;
	}
	struct list_head * iterator;
	struct vaddr_reg * tempNode;
	printk(" start_addr \tsize\n");
	printk("------------\t----\n");
	__list_for_each(iterator, head) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		printk("0x%-10llX\t%llu\n", tempNode->start_address, tempNode->size);
	}
	printk("\n");
}

int table_not_used(u64 table) {
	int i;
	int present = 0;
	u64 * addr = (u64 *)table;
	u64 entry = 0;
	for(i=0; i<512; i++) {
		entry = *addr;
		present = entry & 0x1;
		if(present) {
			return 0;
		}
		addr++;
	}
	return 1;
}

uintptr_t select_victim_addr(struct mem_map * map) {
	
	// variables
	u64 random_iterations;
	u64 i;
	struct list_head * iterator;
	struct vaddr_reg * tempNode;
	
	// init
	random_iterations = 0;
	i = 0;
	iterator = NULL;
	tempNode = NULL;

	// get random number
	//get_random_bytes(&random_iterations, sizeof(u64));
	//random_iterations = random_iterations % 87;
	random_iterations = 0;
	printk("random iterations: %llu\n", random_iterations);

	// get random vaddr_reg
	iterator = &(map->usedListHead);
	for(i=0; i<random_iterations; i++) {
		iterator = iterator->next;
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
	}
	if(iterator == &(map->usedListHead)) {
		iterator = iterator->next;
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
	}

	// get page count of random vaddr_reg
	u64 page_count = tempNode->size / 4096;
	if(tempNode->size % 4096 > 0) {
		page_count += 1;
	}
	printk("page_count: %llu\n", page_count);

	// get random number, restrict by page count
	//get_random_bytes(&random_iterations, sizeof(u64));
	//random_iterations = random_iterations % (page_count);
	//printk("victim_to_use: %llu\n", map->swap->victim_to_use);
	if(map->swap->victim_to_use >= page_count) {
		map->swap->victim_to_use = map->swap->victim_to_use % page_count;
	}	
	random_iterations = map->swap->victim_to_use;
	map->swap->victim_to_use += 1;
	printk("random iterations: %llu\n", random_iterations);
	// get random virtual address in vaddr_reg
	uintptr_t victim_addr = tempNode->start_address;
	for(i=0; i<random_iterations; i++) {
		victim_addr += 4096;
	}

	// pseudorandom user-level virtual address
	return victim_addr;
}

void handle_failed_alloc(struct mem_map * map) {
	printk("in handle_failed_alloc\n");

	// variables
	pml4e64_t * victim_pml4e = NULL;
	pdpe64_t * victim_pdpe = NULL;
	pde64_t * victim_pde = NULL;
	pte64_t * victim_pte = NULL;
	uintptr_t victim_page = (uintptr_t)NULL;
	uintptr_t victim_tempBase = (uintptr_t)NULL;
	uintptr_t victim_tempOff = (uintptr_t)NULL;
	uintptr_t victim_user_vaddr = (uintptr_t)NULL;

	// loop until acceptable victim is found
	// required because select_victim_addr may
	// return an unbacked address
	// if null page, continue
	// if non-null page, break
	while(1) {

		// get pseudorandom victim address
		victim_user_vaddr = select_victim_addr(map);
		printk("victim_user_vaddr: %llX\n", victim_user_vaddr);
	
		// walk page tables for victim page

		// get victim PML4E
		victim_tempBase = CR3_TO_PML4E64_VA(get_cr3());
		victim_tempOff = PML4E64_INDEX(victim_user_vaddr);
		victim_pml4e = victim_tempBase + (victim_tempOff << 3);
		if(victim_pml4e->present == 0) {
			printk("error: pdpe not present\n");
			//return;
			continue;
		}

		// get victim PDPE
		victim_tempBase = BASE_TO_PAGE_ADDR(victim_pml4e->pdp_base_addr);
		victim_tempOff = PDPE64_INDEX(victim_user_vaddr);
		victim_pdpe = __va(victim_tempBase + (victim_tempOff << 3));
		if(victim_pdpe->present == 0) {
			printk("error: pde not present\n");
			//return;
			continue;
		}

		// get victim PDE
		victim_tempBase = BASE_TO_PAGE_ADDR(victim_pdpe->pd_base_addr);
		victim_tempOff = PDE64_INDEX(victim_user_vaddr);
		victim_pde = __va(victim_tempBase + (victim_tempOff << 3));
		if(victim_pde->present == 0) {
			printk("error: pte not present\n");
			//return;
			continue;
		}

		// get victim PTE
		victim_tempBase = BASE_TO_PAGE_ADDR(victim_pde->pt_base_addr);
		victim_tempOff = PTE64_INDEX(victim_user_vaddr);
		victim_pte = __va(victim_tempBase + (victim_tempOff << 3));
		printk("*pte: 0x%llX\n",*(u64 *)victim_pte);
		if(victim_pte->present == 0) {
			printk("error: page not present\n");
			//printk("*pte: 0x%llX\n",*(u64 *)victim_pte);
			//return;
			continue;
		}

		// get victim page
		victim_tempBase = BASE_TO_PAGE_ADDR(victim_pte->page_base_addr);
		victim_tempOff = victim_user_vaddr & 0xFFF;
		victim_page = victim_tempBase + victim_tempOff;
		if((void *)victim_page != (void *)NULL) {
			printk("found victim page\n");
			break;
		}

	} // end of while loop

	// swap page out and clear page contents
	u32 victim_psi = 0;
	if(swap_out_page(map->swap, &victim_psi, (void *)victim_page) != 0) {
		printk("failed on page_swap_out\n");
		return;
	}

	//
	// set victim pte fields:
	//
	//		FIELD				BITS		VALUE
	//		-----------			------		----------------------
	//		present				0			0
	//		victim_psi			[32..1]		32-bit page slot index + 1
	//
	//
	//	   63		33	                  1         0
	//		+---------------------------------------+
	//		| unused |   victim_psi + 1   | present |	TEMPLATE
	//		+---------------------------------------+
	//
	//		+---------------------------------------+
	//		| unused |       psi + 1      |    0    |	SWAPPED-OUT PAGE
	//		+---------------------------------------+
	//
	//
	
	// point pte to swapped out page via page slot index
	memset(victim_pte, 0, 8);
	printk("*pte: 0x%llX\n", *(u64 *)victim_pte);
	victim_psi += 1; 	// avoid problem with index 0
	*((u64 *)victim_pte) = (u64)((victim_psi) << 1); // pte[32..1] = psi + 1
	printk("*pte: 0x%llX\n", *(u64 *)victim_pte);
	victim_pte->present = 0;
	printk("*pte: 0x%llX\n", *(u64 *)victim_pte);

	// free physical page, clear TLB entry
	petmem_free_pages(victim_page, 1);
	invlpg(victim_user_vaddr);
	printk("leaving handle_failed_alloc\n");
}

uintptr_t get_free_page(struct mem_map * map) {
	printk("in get_free_page\n");

	// get page from buddy alloc
	// if none available, swap out
	uintptr_t page_addr = (uintptr_t)NULL;
	while(1) {
		page_addr = petmem_alloc_pages(1);
		printk("after first alloc attempt, page_addr: %p\n", (void *)page_addr);
		if(page_addr) break;
		handle_failed_alloc(map);
		
		// testing only, remove this
		//page_addr = petmem_alloc_pages(1);
		//printk("after second alloc attempt, page_addr: %p\n", (void *)page_addr);
		//break;
	}

	printk("page_addr: %p\n", page_addr);
	printk("leaving get_free_page\n");
	return page_addr;
}

///////////// END OF MY FUNCTIONS //////////////

struct mem_map * petmem_init_process() {
	struct mem_map * map = (struct mem_map *)kmalloc(sizeof(struct mem_map),GFP_KERNEL);
	// project 2 init	
	INIT_LIST_HEAD(&(map->usedListHead));
	INIT_LIST_HEAD(&(map->freeListHead));
	u64 initial_size = PETMEM_REGION_END-PETMEM_REGION_START;
	insert_new_node(PETMEM_REGION_START, initial_size, &(map->freeListHead));
	// project 3 init
	map->swap = swap_init(0);		// place holder, size determined in swap_init
	return map;
}


void petmem_deinit_process(struct mem_map * map) {
	printk("in petmem_deinit_process\n");
	struct list_head * iterator;
	struct vaddr_reg * nextNode;
	// free used list	
	iterator = &(map->usedListHead);
	while(iterator->next != &(map->usedListHead)) {
		nextNode = list_entry(iterator->next, struct vaddr_reg, list_member);
		kfree(nextNode);
		iterator = iterator->next;
	}
	// free free list
	iterator = &(map->freeListHead);
	while(iterator->next != &(map->freeListHead)) {
		nextNode = list_entry(iterator->next, struct vaddr_reg, list_member);
		kfree(nextNode);
		iterator = iterator->next;
	}
	swap_free(map->swap);
	kfree(map->swap);
	kfree(map);
	printk("leaving petmem_deinit_process\n");
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
}

void petmem_free_vspace(struct mem_map * map, uintptr_t vaddr) {

	printk("Free memory\n");
	struct list_head * iterator;
	struct vaddr_reg * tempNode;
	
	int badAddress = 1;
	__list_for_each(iterator, &(map->usedListHead)) {
		tempNode = list_entry(iterator, struct vaddr_reg, list_member);
		if(tempNode->start_address == (u64)(vaddr)) {
			badAddress = 0;
			break;
		}
	}
	if(badAddress) {
		printk("failed in petmem_free_vspace, bad address\n");
		return;
	}

	u64 page_count = (tempNode->size)/4096;
	if(tempNode->size % 4096 != 0) page_count += 1;

	printk("page count: %llu\n", page_count);

	pml4e64_t * pml4e;
	pdpe64_t * pdpe;
	pde64_t * pde;
	pte64_t * pte;

	pml4e64_t * pml4e_first;
	pdpe64_t * pdpe_first;
	pde64_t * pde_first;
	pte64_t * pte_first;

	uintptr_t page;

	int isPresent = 0;

	uintptr_t free_vaddr = vaddr;
	uintptr_t tempBase;
	uintptr_t tempOff;
	u64 i;
	for(i=0; i<page_count; i++) {
		//printk("\nloop iteration: %d\n", i);
	
		// get PML4E
		tempBase = CR3_TO_PML4E64_VA(get_cr3());
		tempOff = PML4E64_INDEX(free_vaddr);
		pml4e = tempBase + (tempOff << 3);
		pml4e_first = tempBase;
		//printk("gets PML4E @ %p\n", pml4e);
		//printk("pml4e:       %llX\n", *pml4e);
		isPresent = (int)(pml4e->present);		
		if(!isPresent) {
			//printk("PDP not present, continuing\n");
			continue;
		}
		
		// get PDPE
		tempBase = BASE_TO_PAGE_ADDR(pml4e->pdp_base_addr);
		tempOff = PDPE64_INDEX(free_vaddr);
		pdpe = __va(tempBase + (tempOff << 3));
		pdpe_first = __va(tempBase);
		//printk("gets PDPE @ %p\n", pdpe);
		//printk("pdpe:       %llX\n", *pdpe);
		isPresent = (int)(pdpe->present);	
		if(!isPresent) {
			//printk("PD not present, continuing\n");
			continue;
		}

		// get PDE
		tempBase = BASE_TO_PAGE_ADDR(pdpe->pd_base_addr);
		tempOff = PDE64_INDEX(free_vaddr);
		pde = __va(tempBase + (tempOff << 3));
		pde_first = __va(tempBase);
		//printk("gets PDE @ %p\n", pde);
		//printk("pde:       %llX\n", *pde);
		isPresent = (int)(pde->present);	
		if(!isPresent) {
			//printk("PT not present, continuing\n");
			continue;
		}

		// get PTE
		tempBase = BASE_TO_PAGE_ADDR(pde->pt_base_addr);
		tempOff = PTE64_INDEX(free_vaddr);
		pte = __va(tempBase + (tempOff << 3));
		pte_first = __va(tempBase);
		//printk("gets PTE @ %p\n", pte);
		//printk("pte:       %llX\n", *pte);
		isPresent = (int)(pte->present);		
		if(!isPresent) {
			//printk("page not present, continuing\n");
			continue;
		}

		// get page
		tempBase = BASE_TO_PAGE_ADDR(pte->page_base_addr);
		tempOff = free_vaddr & 0xFFF;
		page = tempBase + tempOff;
		//printk("gets TLB entry @ %p\n", free_vaddr);
		//printk("gets page @ %p\n", page);

		// invalidate TLB, free physical page
		invlpg(free_vaddr); 
		petmem_free_pages(page, 1);
		//printk("invalidates TLB @ %p\n", free_vaddr);
		//printk("frees page @ %p\n", page);

		// handle PTE
		//pte->present = 0;
		memset(pte, 0, 8);	// this may be wrong or unnecessary

		// check if PT can be freed
		//printk("checks PT @ %p\n", pte_first);
		if(table_not_used(pte_first)) {
			pde->present = 0;
			petmem_free_pages(pte_first,1);
			printk("frees PT @ %p\n", pte_first);

			// check if PD can be freed
			printk("checks PD @ %p\n", pde_first);
			if(table_not_used(pde_first)) {
				pdpe->present = 0;
				petmem_free_pages(pde_first,1);
				printk("frees PD @ %p\n", pde_first);

				// check if PDP can be freed
				printk("checks PDP @ %p\n", pdpe_first);
				if(table_not_used(pdpe_first)) {
					pml4e->present = 0;
					petmem_free_pages(pdpe_first,1);
					printk("frees PDP @ %p\n", pdpe_first);
				}
			}
		}

		// increment freeing address to deal with remaining pages
		free_vaddr += 4096;
	}

	// free virtual address space for user process
	myfree(vaddr, &(map->usedListHead), &(map->freeListHead));
    return;
}


/* 
   error_code:
       1 == not present
       2 == permissions error
*/

int petmem_handle_pagefault(struct mem_map * map, uintptr_t fault_addr, u32 error_code) {
    printk("Page fault!\n");
	printk("fault_addr: %p\n", (void *)fault_addr);

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
		return -1;
	}

	pml4e64_t * pml4e;
	pdpe64_t * pdpe;
	pde64_t * pde;
	pte64_t * pte;

	uintptr_t tempBase = 0;
	uintptr_t tempOff = 0;
	uintptr_t tempAddr = 0;

	// handle PML4E
	tempBase = CR3_TO_PML4E64_VA(get_cr3());
	tempOff = PML4E64_INDEX(fault_addr);
	pml4e = tempBase + (tempOff << 3);
	if(pml4e->present == 0) {
		tempAddr = get_free_page(map);
		if(tempAddr == (uintptr_t)NULL) {
			printk("could not get page for PML4\n");
			return -1;
		}
		memset(__va(tempAddr), 0, 4096);
		pml4e->pdp_base_addr = PAGE_TO_BASE_ADDR(tempAddr);
		pml4e->present = 1;
	}
	pml4e->writable = 1;
	pml4e->user_page = 1;

	// handle PDPE
	tempBase = BASE_TO_PAGE_ADDR(pml4e->pdp_base_addr);
	tempOff = PDPE64_INDEX(fault_addr);
	pdpe = __va(tempBase + (tempOff << 3));
	if(pdpe->present == 0) {
		tempAddr = get_free_page(map);
		if(tempAddr == (uintptr_t)NULL) {
			printk("could not get page for PDP\n");
			return -1;
		}
		memset(__va(tempAddr), 0, 4096);
		pdpe->pd_base_addr = PAGE_TO_BASE_ADDR(tempAddr);
		pdpe->present = 1;
	}
	pdpe->writable = 1;
	pdpe->user_page = 1;

	// handle PDE
	tempBase = BASE_TO_PAGE_ADDR(pdpe->pd_base_addr);
	tempOff = PDE64_INDEX(fault_addr);
	pde = __va(tempBase + (tempOff << 3));
	if(pde->present == 0) {
		tempAddr = get_free_page(map);
		if(tempAddr == (uintptr_t)NULL) {
			printk("could not get page for PD\n");
			return -1;
		}
		memset(__va(tempAddr), 0, 4096);
		pde->pt_base_addr = PAGE_TO_BASE_ADDR(tempAddr);
		pde->present = 1;
	}
	pde->writable = 1;
	pde->user_page = 1;

	// handle PTE
	tempBase = BASE_TO_PAGE_ADDR(pde->pt_base_addr);
	tempOff = PTE64_INDEX(fault_addr);
	pte = __va(tempBase + (tempOff << 3));

	// this case should never happen
	if(pte->present == 1) {
		printk("error: page should not have faulted\n");
		printk("*pte: 0x%llX\n", *(u64 *)pte);
		return -1;
	}

	// get free page
	uintptr_t page_addr = get_free_page(map);

	// testing only, remove this
	if(page_addr == (uintptr_t)NULL) {
		printk("NULL page, get_free_page failed\n");
		return -1;
	}

	// clear page
	memset(__va(page_addr), 0, 4096);

	// check for swapped out page
	// 		present bit is cleared (bit 0)
	//		entire pte = 0
	int swapped_out_page = 0;
	printk("at check for swapped out page\n");
	printk("*pte: %llX\n", (*(u64 *)pte));
	if((*(u64 *)pte) != (u64)0) {
		swapped_out_page = 1;
	}
	printk("swapped_out_page: %d\n", swapped_out_page);
	
	// pte points to swapped out page
	if(swapped_out_page) {
		printk("swap page in from swap file\n");
		u32 psi_from_pte = 0;
		psi_from_pte = (u32)((*(u64 *)pte) >> 1);
		psi_from_pte -= 1;
		printk("psi_from_pte: %llu\n", (u64)psi_from_pte);
		if(swap_in_page(map->swap, psi_from_pte, (void *)page_addr) != 0) {
			printk("swap_in_page failed\n");
		}
	}

	// set page table entry
	pte->writable = 1;
	pte->user_page = 1;
	pte->present = 1;
	pte->page_base_addr = PAGE_TO_BASE_ADDR(page_addr);

    return 0;
}








