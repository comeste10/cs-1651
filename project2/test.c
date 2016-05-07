#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "harness.h"



/* A simple test program. 
 * You should modify this to perform additional checks 
 */

struct box {
	int x;
	int y;
	int z;
};


int main(int argc, char ** argv) {
	
	init_petmem();

	

	// my test
	struct box * b1;
	//struct box * b2;
	//struct box * b3;

	pet_dump();

	b1 = pet_malloc(sizeof(struct box));
	//b2 = pet_malloc(sizeof(struct box));
	//b3 = pet_malloc(sizeof(struct box));

	pet_dump();

	b1->x = 1;
	b1->y = 2;
	b1->z = 3;

	pet_dump();

	printf("before free\n");	

	pet_free(b1);
	//pet_free(b2);
	//pet_free(b3);

	printf("after free\n");

	//pet_dump();
	
	/*
	// SCENARIO 1
    char * buf = NULL;  
    buf = (char *)pet_malloc(8192);
    printf("Allocated buf at %p\n", buf);
    pet_dump();
    buf[50] = 'H';	
    buf[51] = 'e';
    buf[52] = 'l';
    buf[53] = 'l';
    buf[54] = 'o';
    buf[55] = ' ';
    buf[56] = 'W';
    buf[57] = 'o';
    buf[58] = 'r';
    buf[59] = 'l';
    buf[60] = 'd';
    buf[61] = '!';
    buf[62] = 0;
    printf("%s\n", (char *)(buf + 50));
    pet_free(buf);
    printf("%s\n", (char *)(buf + 50));
	*/
    
    return 0;
}
