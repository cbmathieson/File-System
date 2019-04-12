#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "../io/File.h"
//const int BLOCK_SIZE = 512;
//const int NUM_BLOCKS = 4096;
//const int INODE_SIZE = 32;

#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

int main(int argc, char* argv[]) {
    
    //initLLFS returns the inode to the root directory
    int rootDirectory = initLLFS();
    
    printf("current directory inode: %d\n", rootDirectory);
    
    FILE* disk = fopen("../disk/vdisk", "r+b");
    
    //create a couple sub directories in root
    createDirectory(disk, rootDirectory, "folder_1");
    createDirectory(disk, rootDirectory, "folder_2");
    createDirectory(disk, rootDirectory, "folder_3");
    createDirectory(disk, rootDirectory, "folder_4");
    createDirectory(disk, rootDirectory, "folder_5");
    createDirectory(disk, rootDirectory, "folder_6");
    
    //check them out
    printf("after just adding 6 folders: \n");
    listFileNames(disk, rootDirectory);
    printf("\n");
    
    char* testerString = "When wil0 the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east misuses the guns policeman. A supreme addict sighs a lark before the practicable pigeon. The sales When will the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east misuses the guns policeman. A supreme addict sighs a lark before the practicable pigeon. The sales When will the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east misuses the guns policeman. A supreme addict sighs a lark before the practicable pigeon. The sales When will the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east misuses the guns policeman. A supreme addict sighs a lark before the practicable pigeon. The salesWhen will the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east misuses the guns policeman. A supreme addict sighs a lark before the practicable pigeon. The sales.";
    
    char* testerString2 = "The sales When will the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east misuses the guns policeman. A supreme addict sighs a lark before the practicable pigeon. The salesWhen will the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east misuses the guns policeman. A supreme addict sighs a lark before the practicable pigeon. The sales.";
    
    char* testerString3 = "The sales When will the blob speak an amateur? Behind a slash argues this read countryside. The menu withdraws outside a music. The infant chews under a yellow directive. His stopping east.";
    
    //create some files in the root directory
    createFileInDirectory(disk, rootDirectory, "file_1", testerString);
    createFileInDirectory(disk, rootDirectory, "file_2", testerString2);
    createFileInDirectory(disk, rootDirectory, "file_3", testerString3);
    createFileInDirectory(disk, rootDirectory, "file_4", testerString);
    createFileInDirectory(disk, rootDirectory, "file_5", testerString2);
    createFileInDirectory(disk, rootDirectory, "file_6", testerString3);
    
    //check them out
    printf("after adding 6 files\n");
    listFileNames(disk, rootDirectory);
    printf("\n");
    
    //delete a couple files and folders
    deleteFileInDirectory(disk, rootDirectory, "file_1");
    deleteFileInDirectory(disk, rootDirectory, "file_3");
    deleteFileInDirectory(disk, rootDirectory, "file_5");
    deleteDirectory(disk, rootDirectory, "folder_2");
    deleteDirectory(disk, rootDirectory, "folder_4");
    deleteDirectory(disk, rootDirectory, "folder_6");
    
    printf("after deleting 3 files, 3 folders\n");
    listFileNames(disk, rootDirectory);
    printf("\n");
    
    //check out whats in some of the files
    printf("look at the contents of file 2,4,6\n");
    char *file2Text = readFileInDirectory(disk, rootDirectory, "file_2");
    printf("file_2:\n%s\n", file2Text);
    char *file4Text = readFileInDirectory(disk, rootDirectory, "file_4");
    printf("file_4:\n%s\n", file4Text);
    char *file6Text = readFileInDirectory(disk, rootDirectory, "file_6");
    printf("file_6:\n%s\n", file6Text);
    printf("\n");
    
    char* newContent = "we'll keep this short";
    
    printf("Change the contents of the files:\n");
    writeToFileInDirectory(disk, rootDirectory, "file_2", newContent);
    writeToFileInDirectory(disk, rootDirectory, "file_4", newContent);
    writeToFileInDirectory(disk, rootDirectory, "file_6", newContent);
    
    file2Text = readFileInDirectory(disk, rootDirectory, "file_2");
    printf("file_2:\n%s\n", file2Text);
    file4Text = readFileInDirectory(disk, rootDirectory, "file_4");
    printf("file_4:\n%s\n", file4Text);
    file6Text = readFileInDirectory(disk, rootDirectory, "file_6");
    printf("file_6:\n%s\n", file6Text);
    
    //move into a sub directory
    printf("\nNow move into subdirectory folder_1:\n");
    int currentDirectory = openDirectory(disk, rootDirectory, "folder_1");
    if(currentDirectory == -1) {
        printf("could not find subDirectory :(\n");
        return 0;
    }
    
    printf("Adding files and folders in subdirectory:\n");
    createFileInDirectory(disk, currentDirectory, "subfile_1", testerString);
    createFileInDirectory(disk, currentDirectory, "subfile_2", testerString2);
    createFileInDirectory(disk, currentDirectory, "subfile_3", testerString3);
    createDirectory(disk, currentDirectory, "subfolder_1");
    createDirectory(disk, currentDirectory, "subfolder_2");
    createDirectory(disk, currentDirectory, "subfolder_3");
    
    //check them out
    printf("after adding 3 files and 3 folders to subdirectory:\n");
    listFileNames(disk, currentDirectory);
    
    printf("reading sub directories files:\n");
    char* file1Text = readFileInDirectory(disk, currentDirectory, "subfile_1");
    printf("subfile_1:\n%s\n", file1Text);
    char* files2Text = readFileInDirectory(disk, currentDirectory, "subfile_2");
    printf("subfile_2:\n%s\n", files2Text);
    char* file3Text = readFileInDirectory(disk, currentDirectory, "subfile_3");
    printf("subfile_3:\n%s\n", file3Text);
    
    return 0;
}
