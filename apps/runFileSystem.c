#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include "../io/File.h"
//const int BLOCK_SIZE = 512;
//const int NUM_BLOCKS = 4096;
//const int INODE_SIZE = 32;

#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

//globals
//Global Variables
char *argv[512];
int argc = 0;
//initial root directory
int currentDirectory = 11;
FILE* disk;

void purgeArgs(){
    int i = 0;
    
    while(i <= 511) {
        argv[i] = NULL;
        i++;
    }
}

void run_cmd() {
    
    char *command = argv[0];
    
    //if asking for current working directory, print it
    if (strcmp(command, "ls\n") == 0) {
        listFileNames(disk, currentDirectory);
    } else if (strcmp(command, "cd") == 0) {
        //check if .. or <filename>
        strtok(argv[1], "\n");
        if(strcmp(argv[1], "..") == 0) {
            currentDirectory = 11;
        } else {
            int openedDirectory = openDirectory(disk, currentDirectory, argv[1]);
            if(openedDirectory != -1) {
                currentDirectory = openedDirectory;
            }
        }
    } else if(strcmp(command, "mkdir") == 0) {
        strtok(argv[1], "\n");
        createDirectory(disk, currentDirectory, argv[1]);
    } else if(strcmp(command, "rmdir") == 0) {
        strtok(argv[1], "\n");
        deleteDirectory(disk, currentDirectory, argv[1]);
    } else if(strcmp(command, "create") == 0) {
        
        char* file = malloc(BLOCK_SIZE*argc-2);
        for(int i = 2; i < argc; i++) {
            if(i+1 == argc) {
                strtok(argv[i], "\n");
            }
            if(i != 2) {
                strncat(file, " ", 1);
            }
            
            strncat(file, argv[i], BLOCK_SIZE);
        }
        
        createFileInDirectory(disk, currentDirectory, argv[1], file);
        free(file);
        
    } else if(strcmp(command, "update") == 0) {
        
        char* file = malloc(BLOCK_SIZE*argc-2);
        for(int i = 2; i < argc; i++) {
            if(i+1 == argc) {
                strtok(argv[i], "\n");
            }
            if(i != 2) {
                strncat(file, " ", 1);
            }
            
            strncat(file, argv[i], BLOCK_SIZE);
        }
        
        writeToFileInDirectory(disk, currentDirectory, argv[1], file);
        free(file);
        
    } else if(strcmp(command, "read") == 0) {
        strtok(argv[1], "\n");
        char* file;
        file = readFileInDirectory(disk, currentDirectory, argv[1]);
        printf("%s\n", file);
        free(file);
    } else if(strcmp(command, "rm") == 0) {
        strtok(argv[1], "\n");
        deleteFileInDirectory(disk, currentDirectory, argv[1]);
    }
}

void get_args(char* input) {
    char* token = NULL;
    
    token = strtok(input, " ");
    argv[argc] = token;
    
    if(strcmp(token, "exit\n") == 0) {
        printf("exit: exiting process\n");
        exit(0);
    }
    
    //capture rest of input
    for(;;) {
        
        token = strtok(NULL, " ");
        
        argc++;
        
        if (token == NULL) {
            argv[argc] = NULL;
            break;
        }
        
        argv[argc] = token;
        
        //check for exit command
        if(strcmp(token, "exit") == 0 || strcmp(token, "exit\n") == 0) {
            printf("exit: exiting process\n");
            exit(0);
        }
    }
    
    run_cmd();
    
}

void queryUser() {
    char input[514];
    printf("$ ");
    
    if(fgets(input,514,stdin) == NULL){
        printf("Ctrl-D: exiting process\n");
        exit(0);
    }
    
    //check if over 512
    if (strchr(input, '\n') == NULL) {
        fprintf(stderr, "ERROR: Input too long!\n");
        //get rid of extra input
        int c;
        while((c = getc(stdin)) != '\n' && c != EOF);
        return;
    }
    
    //tokenize
    get_args(input);
    
}

int main()
{
    int run = 1;
    
    disk = fopen("../disk/vdisk", "r+b");
    
    printf("Entering file system...\nls -> list directories\ncd .. -> go to root directory\ncd <directoryName> -> enter sub directory\nmkdir <directoryName> -> creates sub directory\nrmdir <directoryName> -> removes sub directory\ncreate <filename> <data> -> creates file and stores data\nupdate <filename> <data> -> updates data in file\nread <filename> -> prints the contents of the file\nrm <filename> -> removes file\nctrl-d or type \"exit\" to finish\n");
    
    while(run){
        
        queryUser();
        //after processing set argc back to 0
        purgeArgs();
        argc = 0;
        
    }
    
    fclose(disk);
    
    return 0;
}
