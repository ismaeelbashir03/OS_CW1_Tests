#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

int main() {
	printf("program %d will run forever, press 'ctrl + c' to interrupt.\n", getpid());
	while (true) {
		int counter = 1;
		for (int x; x < 10000; x++) {
			counter = counter * x;
		}
		sleep(5);
	}			
}
