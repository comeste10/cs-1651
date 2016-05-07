Steve Comer
CS 1651
Project 2
29 Oct 13

Modified files:
	on_demand.h
	on_demand.c
	main.c (printk statements for debugging)
	user/test.c

State of code:
	Gets stuck after pet_free() call in test
	Completes associated ioctl function, called by pet_free()
	Unable to determine why it is stuck here