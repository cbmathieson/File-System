//Just runs a built in function that initialises the file system

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../io/File.h"
//const int BLOCK_SIZE = 512;
//const int NUM_BLOCKS = 4096;
//const int INODE_SIZE = 32;

int main(int argc, char* argv[]) {
	initLLFS();
	return 0;	
}
