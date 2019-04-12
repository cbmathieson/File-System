//do not know how to cache the superblock/freemap, thus very slow performance at the moment
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
const int BLOCK_SIZE = 512;
const int NUM_BLOCKS = 4096;
const int INODE_SIZE = 32;

#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

//--------------------------------------------------------------

/*
 ****ONE FILE PER INODE****
 inode size = 32 bytes
 4 bytes -> size of file (in bytes), lets us know how many blocks are associated on disk
 4 bytes -> flags, type of file ( file [0] or directory [1] )
 10 x 2 bytes -> block numbers for file's first ten blocks
 2 bytes -> single indirect block
 2 bytes -> double indirect block
 */

struct inode
{
    int size;
    int flags;
    int16_t blocks[10];
    int16_t sIndirect;
    int16_t dIndirect;
};

/*
 DIRECTORY
 -> 16 entries
    -> 1st 2 bytes points to inode
    -> Next 30 bytes for filename
 */

struct DirectoryEntry
{
    int16_t inode;
    char filename[30];
};

//--------------------------------------------------------------

/* Helpers to literally read/write blocks from/to vdisk */

//overwrites with new data given to block number given
void writeBlock(FILE* disk, int blockNum, char* data) {
    //moves file pointer to start of block
    fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
    //overwites data in the block
    fwrite(data, sizeof(char), BLOCK_SIZE, disk);
}

//read from disk at file location into buffer
void readBlock(FILE* disk, int blockNum, char* buffer){
    fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
    fread(buffer, sizeof(char), BLOCK_SIZE, disk);
}

//--------------------------------------------------------------

/* Creating, Reading, and Writing files to vdisk */

//creates a file
//Assumes data size less than 5120 bytes
//so we dont need to use indirection... yet
//Basically the first save to the file
int createFile(FILE* disk, char* data) {
    
    int dataSize = strlen(data);
    int16_t blocks[10];
    int blocksToWrite = dataSize/BLOCK_SIZE;
    if(dataSize%BLOCK_SIZE != 0){
        blocksToWrite++;
    }
    
    //get freeblock locations
    int freeBlocks[128];
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fread(freeBlocks, sizeof(int), 128, disk);
    
    for(int i = 0; i < blocksToWrite; i++){
        char* blockBuffer = malloc(sizeof(char)*BLOCK_SIZE);
        
        strncpy(blockBuffer, data + i*BLOCK_SIZE, BLOCK_SIZE);
        
        //find nearest free block through the freemap
        int counter = 0;
        while(TestBit(freeBlocks, counter) != 0){
            counter++;
            if(counter > 4095) {
                printf("ERROR: NO SPACE ON DISK\nABORTING WRITE\n");
            }
        }
        
        //set freeblock bit to 1 since we are now using it
        SetBit(freeBlocks, counter);
        
        //write block and take note where for inode
        writeBlock(disk, counter, blockBuffer);
        
        char* buffer = malloc(512);
        readBlock(disk, counter, buffer);
        
        blocks[i] = counter;
        
        free(buffer);
        free(blockBuffer);
    }
    
    //set reamining blocks to 0
    for(int i = blocksToWrite; i < 10; i++) {
        blocks[i] = 0;
    }
    
    //find one last free block for our inode
    int counter = 10;
    while(TestBit(freeBlocks, counter) != 0) {
        counter++;
    }
    //set bit to 1
    SetBit(freeBlocks, counter);
    
    //update freeblocks on disk
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fwrite(freeBlocks, sizeof(int), 128, disk);
    
    //couldnt figure out why i couldnt pass the array so this is a bit ugly....
    struct inode inode = { dataSize, 0, {blocks[0],blocks[1],blocks[2],blocks[3],blocks[4],blocks[5],blocks[6],blocks[7],blocks[8],blocks[9]}, 0, 0};
    
    //write inode block
    fseek(disk, counter*BLOCK_SIZE, SEEK_SET);
    fwrite(&inode, sizeof(inode), 1, disk);
    
    //-------------------------------------------------------------------------
    
    //increase number of inodes
    int supaBlock[128];
    fseek(disk, 0, SEEK_SET);
    fread(supaBlock, sizeof(int), 128, disk);
    supaBlock[2]++;
    fseek(disk, 0, SEEK_SET);
    fwrite(supaBlock, sizeof(int), 128, disk);
    
    //find first free imap location (5000 means free)
    int16_t imap[256];
    for(int i = 2; i < 10; i++) {
        fseek(disk, (i) * BLOCK_SIZE, SEEK_SET);
        fread(imap, sizeof(int16_t), 256, disk);
        
        for(int j = 0; j < 256; j++) {
            if(imap[j] == 5000) {
                imap[j] = counter;
                fseek(disk, i * 512, SEEK_SET);
                fwrite(imap, sizeof(int16_t), 256, disk);
                return (i-2)*256 + j;
            }
        }
    }
    
    printf("ERROR: UNABLE TO WRITE\n");
    exit(1);
}

//reads file from disk that inode given points to
//Assumes passed buffer is adequately large
//NOT USING INDIRECTION BLOCKS YET! So only up to 5120 bytes as filesize
//ALSO NEED TO WORK WITH DIRECTORIES, ONLY WORKS WITH FILES RN
char* readFile(FILE* disk, int inodeNumber) {
    //makes buffer the size of a block
    struct inode inode;
    
    int imapBlock = inodeNumber/256;
    int imapSpot = inodeNumber%256;
    
    int16_t imap[256];
    
    //reads in imap
    fseek(disk, (2+imapBlock)*BLOCK_SIZE, SEEK_SET);
    fread(imap, sizeof(int16_t), 256, disk);

    int inodeLocation = imap[imapSpot];
    
    //reads inode at specified location
    fseek(disk, inodeLocation * BLOCK_SIZE, SEEK_SET);
    fread(&inode, sizeof(inode), 1, disk);
    
    //get information from inode
    int blocksToRead = inode.size/BLOCK_SIZE;
    if (inode.size%BLOCK_SIZE != 0) {
        blocksToRead++;
    }
    
    //printf("READ blocks to read: %d\n", blocksToRead);
    
    char* file = malloc(sizeof(char)*blocksToRead*BLOCK_SIZE);
    
    //printf("READ inode size: %d\n", inode.size);
    
    memset(file, 0, sizeof(char)*blocksToRead*BLOCK_SIZE);
    //printf("READ leftovers: %d\n", inode.size%BLOCK_SIZE);
    
    for(int i = 0; i < blocksToRead; i++) {
        
        //read new block into full buffer
        
        char* blockBuffer = malloc(sizeof(char)*BLOCK_SIZE);
        memset(blockBuffer, 0, sizeof(char)*BLOCK_SIZE);
        
        readBlock(disk, inode.blocks[i], blockBuffer);
        
        //printf("INODE BLOCK %d:\n%s\n", inode.blocks[i], blockBuffer);
        
        strncpy(file + i*BLOCK_SIZE,blockBuffer, BLOCK_SIZE);
        
        free(blockBuffer);
    }
    
    //printf("FILE: \n%s\n", file);
    
    //NEED TO FREE FILE IN MAIN SINCE WE ALLOCATED EARLIER
    return file;
}

//writes to file
void writeToFile(FILE* disk, int inodeNumber ,char* data){
    //read in file
    struct inode inode;
    
    //find where inode is on imap
    int imapBlock = inodeNumber/256;
    int imapSpot = inodeNumber%256;
    
    int16_t imap[256];
    
    fseek(disk, (2+imapBlock)*BLOCK_SIZE, SEEK_SET);
    fread(imap, sizeof(int16_t), 256, disk);
    
    int inodeLocation = imap[imapSpot];
    
    //reads inode at specified location
    fseek(disk, inodeLocation * BLOCK_SIZE, SEEK_SET);
    fread(&inode, sizeof(inode), 1, disk);
    
    //get information from inode
    int blocksToRead = inode.size/BLOCK_SIZE;
    if (inode.size%BLOCK_SIZE != 0) {
        blocksToRead++;
    }
    
    char* oldVersion = malloc(sizeof(char)*inode.size);
    
    memset(oldVersion, 0, sizeof(char)*inode.size);
    
    for(int i = 0; i < blocksToRead; i++) {
        
        char* blockBuffer = malloc(sizeof(char)*BLOCK_SIZE);
        memset(blockBuffer, 0, sizeof(char)*BLOCK_SIZE);
        
        fseek(disk, inode.blocks[i]*BLOCK_SIZE, SEEK_SET);
        fread(blockBuffer, sizeof(char), BLOCK_SIZE, disk);
        
        memcpy(oldVersion + i*BLOCK_SIZE,blockBuffer, BLOCK_SIZE);
        
        free(blockBuffer);
    }
    
    int blocksNeeded = strlen(data)/BLOCK_SIZE;
    int leftovers = strlen(data)%BLOCK_SIZE;
    if(leftovers != 0){
        blocksNeeded++;
    }
    
    int blocksToWrite[blocksNeeded];
    
    for(int i = 0; i < blocksNeeded; i++) {
        
        int bytesToCheck;
        
        if(leftovers != 0 && i == blocksNeeded-1){
            bytesToCheck = leftovers;
        } else {
            bytesToCheck = BLOCK_SIZE;
        }
        
        if(strncmp(oldVersion + i*BLOCK_SIZE, data + i*BLOCK_SIZE, bytesToCheck) == 0){
            blocksToWrite[i] = 0;
        } else {
            blocksToWrite[i] = 1;
        }
    }
    
    //read in freemap
    
    int freemap[128];
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fread(freemap, sizeof(int), 128, disk);
    
    //find free blocks for blocks that need update
    
    int counter = 10;
    
    //ASSUMES NO INDIRECTION
    for(int i = 0; i < blocksNeeded; i++){
        if(blocksToWrite[i]){
            
            counter = 10;
            
            while(TestBit(freemap, counter) != 0) {
                counter++;
                if(counter > 4095) {
                    printf("ERROR: NOT ENOUGH ROOM ON DISK\nWRITE ABORTED\n");
                    exit(1);
                }
            }
            //change freemap to 0 for old blocks
            ClearBit(freemap, inode.blocks[i]);
            //update inode with new block locations
            inode.blocks[i] = counter;
            //change freemap to 1 for new blocks
            SetBit(freemap, counter);
        }
    }
    
    //find free block for new inode
    
    counter = 10;
    
    while(TestBit(freemap, counter) != 0 ) {
        counter++;
    }
    
    ClearBit(freemap, inodeLocation);
    inodeLocation = counter;
    SetBit(freemap, counter);
    
    //idk why but it keeps setting the bit to 0 at 0
    SetBit(freemap, 0);
    
    //get rid of extra blocks if new file is smaller
    for(int i = 0; i < 10; i++) {
        if(blocksNeeded < i+1) {
            ClearBit(freemap, inode.blocks[i]);
            inode.blocks[i] = 0;
        }
    }
    
    //write freemap to disk
    
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fwrite(freemap, sizeof(int), 128, disk);
    
    for(int i = 0; i < blocksNeeded; i++){
        char* blockBuffer = malloc(sizeof(char)*BLOCK_SIZE);
        strncpy(blockBuffer, data + i*BLOCK_SIZE, BLOCK_SIZE);
        writeBlock(disk, inode.blocks[i], blockBuffer);
        free(blockBuffer);
    }
    
    
    //write inode to disk
    inode.size = (blocksNeeded-1)*BLOCK_SIZE + leftovers;
    
    fseek(disk, inodeLocation*BLOCK_SIZE, SEEK_SET);
    fwrite(&inode, sizeof(inode), 1, disk);
    
    //update inode location on imap
    imap[imapSpot] = inodeLocation;
    
    //write imap to disk
    
    fseek(disk, (2+imapBlock)*BLOCK_SIZE, SEEK_SET);
    fwrite(imap, sizeof(int16_t), 256, disk);
    
    free(oldVersion);
}

void deleteFile(FILE* disk, int inodeNumber) {
    //read in imap
    int imapBlock = inodeNumber/256;
    int imapSpot = inodeNumber%256;
    
    int16_t imap[256];
    
    fseek(disk, (2+imapBlock)*BLOCK_SIZE, SEEK_SET);
    fread(imap, sizeof(int16_t), 256, disk);
    
    //read in inode
    int inodeLocation = imap[imapSpot];
    
    struct inode inode;
    fseek(disk, inodeLocation*BLOCK_SIZE, SEEK_SET);
    fread(&inode, sizeof(inode), 1, disk);
    
    //read in freemap
    int freemap[128];
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fread(freemap, sizeof(int), 128, disk);
    
    int counta = 0;
    for(int i = 0; i < 4096; i++) {
        if(TestBit(freemap, i) != 0) {
            counta++;
        }
    }
    
    //read in superblock
    int suPaBlock[128];
    fseek(disk, 0, SEEK_SET);
    fread(suPaBlock, sizeof(int), 128, disk);
    
    //free nodes in freemap and clear
    for(int i = 0; i < 10; i++) {
        if(inode.blocks[i] != 0) {
            ClearBit(freemap, inode.blocks[i] != 0);
        }
    }
    //clear bit for inode as well
    ClearBit(freemap, inodeLocation);
    //idk why freemap[0] keeps getting set to 0
    SetBit(freemap, 0);
        
    counta = 0;
    
    for(int i = 0; i < 4096; i++) {
        if(TestBit(freemap, i) != 0) {
            counta++;
        }
    }
    
    //remove pointer from imap
    imap[imapSpot] = 5000;
    //lower amount of inodes present in file
    suPaBlock[2]--;
    
    //write to disk
    fseek(disk, 0, SEEK_SET);
    fwrite(suPaBlock, sizeof(int), 128, disk);
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fwrite(freemap, sizeof(int), 128, disk);
    fseek(disk, (imapBlock+2)*BLOCK_SIZE, SEEK_SET);
    fwrite(imap, sizeof(int16_t), 256, disk);
}

//writing directories back in place for consistency
//also not writing directory inodes to imap since you can reach from encapsulating directory
void createDirectory(FILE* disk, int currentDirectory, char* filename) {
    
    if(strlen(filename) > 30) {
        printf("ERROR: FILENAME MUST BE UNDER 30 CHARACTERS\nABORTING MKDIR\n");
        return;
    }
    
    //read in inode
    struct inode inode;
    
    fseek(disk, currentDirectory*BLOCK_SIZE, SEEK_SET);
    fread(&inode, sizeof(inode),1, disk);
    
    if(inode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING MKDIR\n");
        return;
    }
    
    int directoryLocation = inode.blocks[0];
    
    struct DirectoryEntry dir[16];
    
    fseek(disk, directoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&dir, sizeof(dir),1, disk);
    
    //check name isnt already taken
    for(int i = 0; i < 16; i++) {
        if(strncmp(filename, dir[i].filename, 30) == 0){
            printf("ERROR: NAME ALREADY TAKEN\nABORTING MKDIR\n");
            return;
        }
    }
    
    int availableFile = 20;
    
    //find available nodes
    for(int i = 0; i < 16; i++) {
        if(dir[i].inode == 0) {
            availableFile = i;
            break;
        }
    }
    
    //check if available nodes
    if(availableFile == 20) {
        printf("ERROR: ALL 16 SPACES ARE TAKEN IN THIS DIRECTORY\n");
        printf("ABORTING MKDIR\n");
        return;
    }
    
    int freemap[128];
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fread(freemap, sizeof(int),128, disk);
    
    //find free space
    int availableBlock1 = 5000;
    int availableBlock2 = 5000;
    
    for(int i = 10; i < 4096; i++) {
        if(TestBit(freemap, i) == 0) {
            availableBlock1 = i;
            SetBit(freemap, i);
            break;
        }
    }
    for(int i = 10; i < 4096; i++) {
        if(TestBit(freemap, i) == 0) {
            availableBlock2 = i;
            SetBit(freemap, i);
            break;
        }
    }
    
    //idk why it keeps getting set to 0 at [0]
    SetBit(freemap, 0);
    
    //if none abort
    if(availableBlock1 == 5000 || availableBlock2 == 5000) {
        printf("ERROR: NOT ENOUGH SPACE ON DISK\nABORTING MKDIR\n");
        return;
    }
    
    //now write data in
    struct inode newInode = {512, 1, {availableBlock1,0,0,0,0,0,0,0,0,0}, 0, 0};
    
    dir[availableFile].inode = availableBlock2;
    strncpy(dir[availableFile].filename, filename, 30);
    
    //create new directory
    struct DirectoryEntry newDir[16];
    
    for(int i = 0; i < 16; i++) {
        newDir[i].inode = 0;
        strncpy(newDir[i].filename, "", 30);
    }
    
    //write encapsulating directory back using directoryLocation
    //(we arent changing directory location on write)
    fseek(disk, directoryLocation*BLOCK_SIZE, SEEK_SET);
    fwrite(&dir, sizeof(dir),1, disk);
    //write new directory to disk
    fseek(disk, availableBlock1*BLOCK_SIZE, SEEK_SET);
    fwrite(&newDir, sizeof(newDir),1, disk);
    //write new directory inode to disk
    fseek(disk, availableBlock2*BLOCK_SIZE, SEEK_SET);
    fwrite(&newInode, sizeof(newInode),1, disk);
    //write freemap to disk
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fwrite(freemap, sizeof(int),128, disk);
}

int openDirectory(FILE* disk, int parentDirectory, char* subDirectoryName) {
    
    //read in parent directory inode
    struct inode parentDirectoryInode;
    
    fseek(disk, parentDirectory*BLOCK_SIZE, SEEK_SET);
    fread(&parentDirectoryInode, sizeof(parentDirectoryInode),1, disk);
    
    if(parentDirectoryInode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING CD\n");
        return -1;
    }
    
    int parentDirectoryLocation = parentDirectoryInode.blocks[0];
    
    //read in parent directory
    struct DirectoryEntry parentDir[16];
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&parentDir, sizeof(parentDir),1, disk);
    
    int subDirectoryInodeLocation = 20;
    
    for(int i = 0; i < 16; i++) {
        if(strncmp(subDirectoryName, parentDir[i].filename, 30) == 0) {
            subDirectoryInodeLocation = parentDir[i].inode;
        }
    }
    
    if(subDirectoryInodeLocation == 20) {
        printf("directory doesn't exist!");
        return -1;
    } else {
        return subDirectoryInodeLocation;
    }
    
}

void deleteDirectory(FILE* disk, int parentDirectory, char* subDirectoryName) {
    
    //read in parent directory inode
    struct inode parentDirectoryInode;
    
    fseek(disk, parentDirectory*BLOCK_SIZE, SEEK_SET);
    fread(&parentDirectoryInode, sizeof(parentDirectoryInode),1, disk);
    
    if(parentDirectoryInode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING RMDIR\n");
        return;
    }
    
    int parentDirectoryLocation = parentDirectoryInode.blocks[0];
    
    //read in parent directory
    struct DirectoryEntry parentDir[16];
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&parentDir, sizeof(parentDir),1, disk);
    
    int subDirectoryInodeLocation = 20;
    int subDirectorySpotInParentDirectory = 20;
    
    for(int i = 0; i < 16; i++) {
        if(strncmp(subDirectoryName, parentDir[i].filename, 30) == 0) {
            subDirectoryInodeLocation = parentDir[i].inode;
            subDirectorySpotInParentDirectory = i;
        }
    }
    
    if(subDirectorySpotInParentDirectory == 20 || subDirectoryInodeLocation == 20) {
        printf("file doesn't exist!");
        return;
    }
    
    //read in subDirectory inode
    
    struct inode subDirectoryInode;
    fseek(disk, subDirectoryInodeLocation*BLOCK_SIZE, SEEK_SET);
    fread(&subDirectoryInode, sizeof(subDirectoryInode),1, disk);
    
    if(subDirectoryInode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING RMDIR\n");
        return;
    }
    
    int subDirectoryBlock = subDirectoryInode.blocks[0];
    
    //update freemap
    int freemap[128];
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fread(freemap, sizeof(int),128, disk);
    
    ClearBit(freemap, subDirectoryInodeLocation);
    ClearBit(freemap, subDirectoryBlock);
    
    fseek(disk, 1*BLOCK_SIZE, SEEK_SET);
    fwrite(freemap, sizeof(int),128, disk);
    
    //update parent directory
    parentDir[subDirectorySpotInParentDirectory].inode = 0;
    strncpy(parentDir[subDirectorySpotInParentDirectory].filename, "", 30);
    
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fwrite(&parentDir, sizeof(parentDir),1, disk);
    
    //no need to remove pointers from inode since the inode itself will no longer be pointed to
}

//pass this the inode of the directory you want the listings of
void listFileNames(FILE* disk, int directory) {
    //read in inode
    struct inode inode;
    
    fseek(disk, directory*BLOCK_SIZE, SEEK_SET);
    fread(&inode, sizeof(inode),1, disk);
    
    if(inode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING\n");
        return;
    }
    
    int directoryLocation = inode.blocks[0];
    
    struct DirectoryEntry dir[16];
    
    fseek(disk, directoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&dir, sizeof(dir),1, disk);
    
    for(int i = 0; i < 16; i++) {
        if(dir[i].inode != 0) {
            printf("%s\n", dir[i].filename);
        } else {
            //printf("file location %d is empty\n", i);
        }
    }
}

char* readFileInDirectory(FILE* disk, int currentDirectoryInode, char* filename) {
    
    struct inode parentDirectoryInode;
    
    fseek(disk, currentDirectoryInode*BLOCK_SIZE, SEEK_SET);
    fread(&parentDirectoryInode, sizeof(parentDirectoryInode),1, disk);
    
    if(parentDirectoryInode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING\n");
        return "";
    }
    
    int parentDirectoryLocation = parentDirectoryInode.blocks[0];
    
    struct DirectoryEntry parentDir[16];
    
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&parentDir, sizeof(parentDir),1, disk);
    
    int fileimapLocation = 5000;
    
    for(int i = 0; i < 16; i++) {
        if(strncmp(parentDir[i].filename, filename, 30) == 0) {
            fileimapLocation = parentDir[i].inode;
            break;
        }
    }
    
    if(fileimapLocation == 5000) {
        printf("ERROR: FILE DOES NOT EXIST\nABORTING READ\n");
        return "";
    }
    
    return readFile(disk, fileimapLocation);
    
}

void writeToFileInDirectory(FILE* disk, int currentDirectoryInode, char* filename, char* data) {
    
    struct inode parentDirectoryInode;
    
    fseek(disk, currentDirectoryInode*BLOCK_SIZE, SEEK_SET);
    fread(&parentDirectoryInode, sizeof(parentDirectoryInode),1, disk);
    
    if(parentDirectoryInode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING WRITE\n");
        return;
    }
    
    int parentDirectoryLocation = parentDirectoryInode.blocks[0];
    
    struct DirectoryEntry parentDir[16];
    
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&parentDir, sizeof(parentDir),1, disk);
    
    int fileimapLocation = 5000;
    
    for(int i = 0; i < 16; i++) {
        if(strncmp(parentDir[i].filename, filename, 30) == 0) {
            fileimapLocation = parentDir[i].inode;
            break;
        }
    }
    
    if(fileimapLocation == 5000) {
        printf("ERROR: FILE DOES NOT EXIST\nABORTING READ\n");
        return;
    }
    
    writeToFile(disk, fileimapLocation, data);
}

void createFileInDirectory(FILE* disk, int currentDirectoryInode, char* filename, char* data) {
    
    struct inode parentDirectoryInode;
    
    fseek(disk, currentDirectoryInode*BLOCK_SIZE, SEEK_SET);
    fread(&parentDirectoryInode, sizeof(parentDirectoryInode),1, disk);
    
    if(strlen(filename) > 30) {
        printf("ERROR: FILE NAME MUST BE UNDER 30 CHARACTERS\nABORTING FILE CREATION\n");
        return;
    }
    
    if(parentDirectoryInode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING\n");
        return;
    }
    
    int parentDirectoryLocation = parentDirectoryInode.blocks[0];
    
    struct DirectoryEntry parentDir[16];
    
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&parentDir, sizeof(parentDir),1, disk);
    
    int availableFile = 20;
    
    for(int i = 0; i < 16; i++) {
        if(parentDir[i].inode == 0) {
            availableFile = i;
            break;
        }
    }
    
    for(int i = 0; i < 16; i++) {
        if(strncmp(parentDir[i].filename, filename, 30) == 0) {
            printf("ERROR: FILENAME ALREADY EXISTS\nABORTING CREATE FILE\n");
            return;
        }
    }
    
    if(availableFile == 20) {
        printf("ERROR: NO MORE SPACE IN DIRECTORY\nABORTING FILE CREATION\n");
        return;
    }
    
    int fileImapLocation = createFile(disk, data);
    
    parentDir[availableFile].inode = fileImapLocation;
    strncpy(parentDir[availableFile].filename, filename, 30);
    
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fwrite(&parentDir, sizeof(parentDir), 1,disk);
    
}

void deleteFileInDirectory(FILE* disk, int currentDirectoryInode, char* filename) {
    
    struct inode parentDirectoryInode;
    
    fseek(disk, currentDirectoryInode*BLOCK_SIZE, SEEK_SET);
    fread(&parentDirectoryInode, sizeof(parentDirectoryInode),1, disk);
    
    if(parentDirectoryInode.flags == 0) {
        printf("ERROR: NOT A DIRECTORY\nABORTING DELETE\n");
        return;
    }
    
    int parentDirectoryLocation = parentDirectoryInode.blocks[0];
    
    struct DirectoryEntry parentDir[16];
    
    fseek(disk, parentDirectoryLocation*BLOCK_SIZE, SEEK_SET);
    fread(&parentDir, sizeof(parentDir),1, disk);
    
    int fileimapLocation = 5000;
    int spotInDir = 5000;
    
    for(int i = 0; i < 16; i++) {
        if(strncmp(parentDir[i].filename, filename, 30) == 0) {
            fileimapLocation = parentDir[i].inode;
            spotInDir = i;
            break;
        }
    }
    
    if(fileimapLocation == 5000 || spotInDir == 5000) {
        printf("ERROR: FILE DOES NOT EXIST\nABORTING DELETE\n");
        return;
    }
    
    deleteFile(disk, fileimapLocation);
    
    strncpy(parentDir[spotInDir].filename, "", 30);
    parentDir[spotInDir].inode = 0;
    
    fseek(disk, parentDirectoryLocation * BLOCK_SIZE, SEEK_SET);
    fwrite(&parentDir, sizeof(parentDir), 1, disk);
    
}

//--------------------------------------------------------------

/* Initialise the file system! */
int initLLFS(){
    FILE* disk = fopen("../disk/vdisk", "w+b");
    
    //create superblock
    int superblock[128];
    superblock[0] = 420; //magic number heh
    superblock[1] = 4096; //blocks on disk
    superblock[2] = 1; //currently 1 inode on disk (root directory)
    
    for(int i = 3; i < 128; i++) {
        superblock[i] = 0;
    }
    
    //write to block
    fseek(disk, 0, SEEK_SET);
    fwrite(superblock, sizeof(int), 128, disk);
    
    //create free block vector
    //the three functions defined at the top allow us to set individual bits and check them
    int freeBlocks[128];
    //clear bit array
    for(int i = 0; i < 128; i++) {
        freeBlocks[i] = 0;
    }
    //set first 10 to not free
    for(int i = 0; i < 10; i++) {
        SetBit(freeBlocks, i);
    }
    
    //take two more blocks for the root directory/inode
    SetBit(freeBlocks, 10);
    SetBit(freeBlocks, 11);
    
    fseek(disk, 1 * BLOCK_SIZE, SEEK_SET);
    fwrite(freeBlocks, sizeof(int), 128, disk);
    
    /*
     -> 2-9 blocks designated for imap pointers.
     -> these will be 16 bit ints that describe the block location of each inode sequentially
     -> since int16_t = 2 bytes, 512/2 = 256. 256*8 blocks = 2048
     This happens to be just over the max we could have!
     (1 data block/1 inode => 4096-10 = 4086/2 = 2043 possible inodes)
     */
    
    //create and write root
    struct DirectoryEntry root[16];
    
    for(int i = 0; i < 16; i++) {
        root[i].inode = 0;
        strncpy(root[i].filename, "", 30);
    }
    
    fseek(disk, 10*BLOCK_SIZE, SEEK_SET);
    fwrite(root, sizeof(root), 1, disk);
    
    struct inode rootInode = {512, 1, {10,0,0,0,0,0,0,0,0,0}, 0, 0};
    fseek(disk, 11*BLOCK_SIZE, SEEK_SET);
    fwrite(&rootInode, sizeof(rootInode),1,disk);
    
    //if set to 5000, no designation. Otherwise that's the inode blocks location on disk
    int16_t imap[256];
    
    //this is the first inode for the root directory
    imap[0] = 11;
    
    for(int i = 1; i < 256; i++){
        imap[i] = 5000;
    }
    
    //assign the empty imap values to remaining reserved blocks
    for(int i = 2; i < 10; i++) {
        fseek(disk, i * BLOCK_SIZE, SEEK_SET);
        fwrite(imap, sizeof(int16_t), 256, disk);
    }
    fclose(disk);
    
    return 11;
    
}
