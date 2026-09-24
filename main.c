#include <stdio.h>
#include <string.h>
#include "walloc.h"

int main(void){
	char *a = walloc(32);
	strcpy(a, "hello");
	printf("%s\n", a);

	winfree(a);
	printf("done\n");
	return 0;
}
