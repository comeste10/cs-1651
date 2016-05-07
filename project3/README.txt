Steve Comer
CS 1651
Project 3 - Swapping
4 Dec 13

Modified files:
	on_demand.h
	on_demand.c
	swap.h
	swap.c
	user/test.c

State of code:
	Works correctly according to provided tests
	
How to run:
	cd petmem
	./run_test.sh
	
Test file:
	
	Variable parameters
		
		int pagestocheck:	Number of checks to perform (see test.c)
							Roughly corresponds to number of swap-in operations performed
		
		int morethanmax:	128MB pool supports 32768 pages, 32703 usable (65 for page tables)
							Parameter indicates number of pages to allocate beyond 32703
							Roughly corresponds to number of swap-out operations performed
							
	Output
	
		Indicates value of 5 locations printed to stdout
			buf[0, 4096, 8192, total_bytes-8192, total_bytes-4096]
			all values printed should be 'a'
			
		X - number of times swap_out_page called
		X - number of times swap_in_page called
		
		Note: 	last two numbers are only accurate for small values
				because they rely on dmesg, which is overwritten by the
				large number of printk statements used in the program
				for large numbers of pages
				
				to check realistic counts, remove 'pet_free(buf);' on
				line 53 of test.c