#include <stdlib.h>
#include <stdio.h>

inline void error(char *msg) {
	printf("Error: %s\n", msg);
	exit(1);
}
