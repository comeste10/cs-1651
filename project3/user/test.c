#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>

#include "harness.h"


/* A simple test program. 
 * You should modify this to perform additional checks 
 */


int main(int argc, char ** argv) {
	
	init_petmem();

	int pagestocheck = 2000;			// ~ swap-in operations
	int morethanmax = 253;				// ~ swap-out operations
	int total_pages = 32703 + morethanmax;
	unsigned long long total_bytes = 4096 * total_pages;
	char * buf = (char *)pet_malloc(total_bytes);
	unsigned long long current_index;
	int i;

    printf("Allocated buf at %p\n", buf);

	// back each page
	current_index = 0;
	for(i=0; i<total_pages; i++) {
		current_index = 4096*i;
		buf[current_index] = 'a';
	}

	// check each page
	current_index = 0;
	for(i=0; i<pagestocheck; i++) {
		current_index = 4096*i;
		if(buf[current_index] != 'a') {
			printf("Error: incorrect value in page %d\n", i);
			break;
		}
	}


	printf("buf[0]:     %c\n", buf[0]);
	printf("buf[4096]:  %c\n", buf[4096]);
	printf("buf[8192]:  %c\n", buf[8192]);
	printf("buf[prev]:  %c\n", buf[total_bytes-8192]);
	printf("buf[last]:  %c\n", buf[total_bytes-4096]);

    pet_free(buf);
	
	pet_dump();
   
    return 0;
}
