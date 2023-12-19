#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include "disk_emu.c"

// We have 1024 data blocks of 1024 bytes each
//datablock[0]:SuperBlock (1 SuperBlock)
//datablock[1]: Root Directory inode (1 Root Directory inode)
//datablock[2] to datablock[101]: Inodes (101 inodes but inode 0 points to root directory) so index goes from 1 to 100 (ALWAYS +1)
//databloc[102] to datablock[1022]: Data (921 blocks) so index goes from 0 to 920(ALWAYS +102)
//datablock[1023]:freebit map (1 block)

// ################## IN-MEMORY DATA STRUCTURES ########################
// ------------------------ Open File Decriptor Table ------------------
// fd_entry structure to be stored in fd_table
typedef struct fd_entry {
    int node_num;
    int rw;
}fd_entry;

// Initialize open file descriptor table
fd_entry *fd_table;
char *openfiles;
// ------------------------ Directory Cache ----------------------------
// dir_entry structure to be stored in directory (on disk) and cached in memory
typedef struct dir_entry {
    char filename[19];
    int node_num;
}dir_entry;

// Directory cache with files contained in root directory
dir_entry *directory_cache;
char *directory_space;

// ------------------------ I-node Cache -------------------------------
// I-node structure
typedef struct inode {
        int mode;
        int linkcnt;
        int uid;
        int gid;
        int size;
        int p[12];
        int ind_p[11];
}inode;

// Inode Table Cache, when we allocate an inode to a file we add the entry to the cache so we know which ones are available

inode *inode_cache;
char *freeinodes;
int indirect_array [1101][92];
int indirect_array_space [1101];

// ------------------------ Freebit Map Cache  ----------------------------
char *diskblock_cache;

// ################## ON DISK DATA STRUCTURES ####################
// Superblock structure
typedef struct super_block {
        char magic[16];
        int blocksize;
        int nblocks;
        int nodelength;
        int root;
}super_block;

//---------------------- HELPER FUNCTIONS ----------------------------------------------------------
//Function to check if a datablock is full returns -1 if it is or a pointer to end of file if it's not
int is_full (char * lol, int size) {
    int s = -1;
    for (int i = 0; i < size; i++){
        if(lol[i]==0){
            s = i;
            break;
        }
    }
    return s;
}

int free_space () {
    int s = -1;
    for (int k =0; k <921;k++) {
   
        if( diskblock_cache[k]== 0 ) {
            s = k+102;
            break;
        }
    }
    return s;
}

int free_inode () {
    int s = -1;
    for (int k =0; k <101;k++) {
        if( freeinodes[k]== 0 ) {
            s = k+1;
            break;
        }
    }
    return s;
}

int free_directory () {
    int s = -1;
    for (int k =0; k < 100;k++) {
        if( directory_space[k]== 0 ) {
           s = k;
           break;
        }
    }
    return s;
}

int free_fd () {
    int s = -1;
    for (int k =0; k <100;k++) {
        if( openfiles[k]== 0 ) {
            s=k;
            break;
        }
    }
    return s;
}

int free_indirect () {
    int s = -1;
    for (int k =1; k <1101;k++) {
        if( indirect_array_space[k]== 0 ) {
            s=k;
            break;
        }
    }
    return s;
    
}

// ---------------------- SFS Functions -----------------------------------
void mksfs(int fresh){
    // Initializing a fresh disk
    if (fresh == 1) {
        init_fresh_disk("disk",1024,1024);
    
    // Initializing On-Disk and In-Memory structures

    // --------- SuperBlock Initialization ---------
    //Buffer to  write struct contents to disk
    char * superblock = (char*) calloc(sizeof(super_block),1);
    super_block *s = (super_block *)superblock;
    strcpy(s->magic, "0xACBD0005");
    s->blocksize = 1024;
    s->nblocks = 1024;
    s->nodelength = 101;
    s->root = 0;
    
    // WRITING DATA TO DISK (FLUSH TO DISK)
    write_blocks(0,1, superblock);
    //Free allocated memory for buffer to write to disk
    free(superblock);
    
    // Allocating and initializing I-node pointing to root directory
    // mode read 0, write 1, read and write 2.
    //Buffer to  write struct contents to disk
    char *home = (char *) calloc(sizeof(inode),1);
    inode *h = (inode *)home;
    h->mode = 2;
    h->linkcnt = 1;
    h->uid = 5201;
    h->gid = 3405;
    h->size = 0;
    
    // WRITTING DATA TO DISK (FLUSH TO DISK)
    write_blocks(1,1,home);
    //Free allocated memory for buffer to write to disk
    free(home);
    
    // Adding entry to inode table to MEMORY
    inode_cache = (inode *) calloc (101,sizeof(inode));
    inode_cache[0] = *h;


    //Initializing free inodes array
    freeinodes = (char *)calloc(101,1);
    freeinodes[0] = 1;

    // Initialzing Free BitMap 1 means occupied and 0 means free
    //Index from 0 to 920 we add 102 to get ACTUAL DATABLOCK
    diskblock_cache = (char*) calloc (921,1);
    
    // Writing data to DISK
    write_blocks(1023,1, diskblock_cache);
    
    // Initializing open file descriptor table
    fd_table = (fd_entry *) calloc(100,sizeof(fd_entry));
    openfiles = (char *) calloc (100,1);
    
    // Initializing directory cache (files contained in the root directory)
    directory_cache = (dir_entry *)calloc(100,sizeof(dir_entry));
    directory_space = (char *) calloc (100, 1);
    }
    // Else we restart an existing disk
    else {
        init_disk("disk",1024,1024);
        
        //Reloading data into caches
        // Initializing open file descriptor table
        fd_table = (fd_entry *) calloc(100,sizeof(fd_entry));
        openfiles = (char *) calloc (100,1);
        
        
    }
}

int sfs_getnextfilename(char* fname) {
    int found = 0;
    if (strcmp(fname,"") == 0){
        for (int i=0; i <100; i++){
            if (directory_space[i]== 1){
                if (strcpy(fname, directory_cache[i].filename) == 0){
                    found = 1;
                    break;
                }
                }
            }
        return found;
        }
        else {
        for (int i=0; i <100; i++){
            if (directory_space[i]== 1){
                if (strcmp(directory_cache[i].filename, fname) == 0) {
                    for (int j = i+1; j < 100; j ++) {
                        if( directory_space[j] == 1) {
                            strcpy(fname, directory_cache[j].filename);
                            found = 1;
                            break;
                        }
                    }//End of inner for loop
                }
                
            }
        } //End of for loop
        
        return found;
    }
}

// ----------------- OPEN FILE --------------------------------------------------
int sfs_fopen (char* fname) {
// Find if there is space in open file descriptor table.
// Getting fd descriptor of fd_table
    int fd = -1;
    for (int i = 0; i < 100; i ++){
        if (openfiles[i]==0){
            fd = i;
            break;
        }
    }
    //No space in open file descriptor table so we return -1 and display error message
    if (fd == -1) {
        //printf("File can't be opened: No space in open file descriptor table\n");
        return -1;
    }
    if (strlen(fname) >19){
        return -1;
    }
          
//Checking if it exists and if it doesn't, can it be added to root directory?
    //1.-Get inode pointing to root directory
    //2.- Get size of root directory
    int size_root = inode_cache[0].size;
    // If size is 0, we allocate a datablock from disk to write in it
    if (size_root == 0) {
    //Make p[0] point to datablock [where + 102], store change in inode_cache and write change to inode to disk as well
    //Initialize inode for file that we want to open
    //Write dir_entry into directory cache and directory datablock on disk
    //Mark diskblock_cache entry as utilized -> set as 1
    //Update size of rootdirectory to 1
    //Write to datablocks [102],[103] and [104] on disk
        inode_cache[0].p[0] = 102;
        inode_cache[0].p[1] = 103;
        inode_cache[0].p[2] = 104;
        inode_cache[0].size = 3*1024;
        
        char* buffer = (char*)calloc(sizeof(inode),1);
        inode* t = (inode*) buffer;
        t->mode = inode_cache[0].mode;
        t->linkcnt = inode_cache[0].linkcnt;
        t->uid = inode_cache[0].uid;
        t->gid = inode_cache[0].gid;
        t->size = inode_cache[0].size;
        t->p[0] = inode_cache[0].p[0];
        t->p[1] = inode_cache[0].p[1];
        t->p[2] = inode_cache[0].p[2];
        write_blocks(1,1, buffer);
        free(buffer);
        
        //Mark datablock as used in the diskblock cache, we use first data block since it is the first time we open a file
        diskblock_cache[0] = 1;
        diskblock_cache[1] = 1;
        diskblock_cache[2] = 1;
        
        write_blocks(1023,1, diskblock_cache);
        
        //Initialize inode for file that we are trying to open, since this is the first time we write to directory
        //We can use 1st inode allocated for files
        char* buffer_1 = (char *)calloc(sizeof(inode),1);
        inode* f = (inode*) buffer_1;
        f->mode = 2;
        f->linkcnt = 1;
        f->uid = 5201;
        f->gid = 3405;
        f->size = 0;
        write_blocks(2,1, buffer_1);
        inode_cache[1] = *f;
        freeinodes[1] = 1;

        free(buffer_1);
        
        //Create fd_entry to store in fd_table mark fd_entry as taken in openfiles array
        //Marke same index as 1 in openfiles array
        char * buffer_2 = (char *) calloc(sizeof(fd_entry),1);
        fd_entry *e = (fd_entry*)buffer_2;
        e->node_num = 2;
        e->rw = 0;
        fd_table[0] = *e;
        openfiles[0] = 1;
        free(buffer_2);
        
        //Create dir_entry to store in directory_cache
        //Use buffer to write dir_entry into datablock in disk which is pointed by inode which points to root directory
        char *buffer_3 = (char*) calloc(sizeof(dir_entry),1);
        dir_entry* d = (dir_entry*)buffer_3;
        strcpy(d->filename,fname);
        d->node_num = 2;
        directory_cache[0] = *d;
        directory_space[0] = 1;
        
        write_blocks(102,1, buffer_3);
        free(buffer_3);
        return fd;
    }
    //If size IS NOT 0
    //We first check if file is already contained in root directory, then we open it in append mode
    //If not we check assigned entries in root directory, and compare filenames to directory_cache dir_entry.filename
    //If IT EXISTS IN DIRECTORY ALREADY then we open file (by adding fd_entry to fd_table) and rw pointer pointing at first free bit within datablock
    //If its full and pointer is p[12], we initialize indirect data block
    //If its full and we are dealing with indirect pointer already we set rw to -1 which means the file is full
    else {
        int found = -1;
        int index = 0;
        for (int i = 0; i < 100; i++) {

            //Check if anyfile already in directory matches the filename
            if (directory_space[i] == 1) {
                found = strcmp(directory_cache[i].filename, fname);
                if (found == 0){
                    index = i;
                    break;
                }
            }
        }
        //If not found then we create and open a new file
        //1.- Create and allocate inode in cache and disk table
        //3.-Add fd_entry to fd_table
        //4.-Add dir_entry to directoy_cache(memory), check which is the first empty entry in the directory and write contents to disk using a buffer into datablocks assigned to root directory(disk)
        if (found != 0 ) {
            // Allocating and initializing I-node pointing to root directory
            // mode read 0, write 1, read and write 2.
            //Buffer to  write struct contents to disk
            char *new_file = (char *) calloc(sizeof(inode),1);
            inode* new_f = (inode*)new_file;
            new_f->mode = 2;
            new_f->linkcnt = 1;
            new_f->uid = 5201;
            new_f->gid = 3405;
            new_f->size = 0;
            
            int node_place = free_inode();
            if (node_place == -1){
               // printf("No inodes left, can't open file\n");
                return -1;
            }
            

            // Adding entry to inode table to MEMORY
            write_blocks(node_place,1, new_file);
            inode_cache[node_place-1] = *new_f;
            freeinodes[node_place-1] = 1;
            free(new_file);
            
            int directory_place = free_directory();
            if (directory_place == -1){
                return -1;
            }

            //Create dir_entry to store in directory_cache
            //Use buffer to write dir_entry into datablock in disk which is pointed by inode which points to root directory
            char * buffer_5 = (char *)calloc(sizeof(dir_entry),1);
            dir_entry* d = (dir_entry*)buffer_5;
            strcpy(d->filename,fname);
            d->node_num = node_place;
            
            directory_cache[directory_place] = *d;
            directory_space[directory_place] = 1;
      
            int which_block = floor(directory_place/42.0);
            int which_position = directory_place % 42;

            int blok = inode_cache[0].p[which_block];
            char before [1024];
            read_blocks (blok,1, before);
            //Adding directory entry to directory
            for (int i = 0; i < 24; i++) {
                before[(which_position*24)+i] = buffer_5[i];
            }
        
            write_blocks(blok,1, before);
            free(buffer_5);
            
            //Create fd_entry to store in fd_table mark fd_entry as taken in openfiles array
            //Marke same index as 1 in openfiles array
            
            int fd_place = free_fd();
            if (fd_place == -1){
               // printf("No space in open file descriptor table left, can't open file\n");
                return -1;
            }
            
            char *buffer_6 = (char*) calloc(sizeof(fd_entry),1);
            fd_entry *e_new = (fd_entry*)buffer_6;
            e_new->node_num = node_place;
            e_new->rw = 0;
            fd_table[fd_place] = *e_new;
            openfiles[fd_place] = 1;
            free(buffer_6);
            
            return fd_place;

        }
        //If found then we open file in append mode(rw pointer set to the end of file)
        else {
  
            int node_local =  directory_cache[index].node_num;
            //CHECK IF IT'S NOT OPENED ALREADY
            
            for (int i =0; i < 100; i++){
                if(openfiles[i] ==1){
                    if (fd_table[i].node_num == node_local){
                        return -1;
                    }
                }
                
            }
            //Locate LAST ASSIGNED POINTER in inode
            int size_local =inode_cache[node_local-1].size;
            if (size_local == 0) {
        
                int fd_2 = free_fd();
                if (fd_2 == -1) {
    
                    return -1;
                }
                char *buffer_4 = (char*)calloc(sizeof(fd_entry),1);
                fd_entry *v = (fd_entry*)buffer_4;
                v->node_num = node_local;
                v->rw = 0;
                fd_table[fd_2] = *v;
                openfiles[fd_2] = 1;
                free(buffer_4);
                return fd_2;
                
            }
            
            int append_pointer = size_local;
            int fd_2 = free_fd();
        
            if (fd_2 == -1) {
               // printf("No space in open file descriptor table left, can't open file\n");
                return -1;
            }
            
            char * buffer_4 = (char*)calloc(sizeof(fd_entry),1);
            fd_entry* v = (fd_entry*)buffer_4;
            v->node_num = node_local;
            v->rw = append_pointer;
            fd_table[fd_2] = *v;
            openfiles[fd_2] = 1;
            free(buffer_4);
            return fd_2;
        }
    }
}
//Iterate directory, find file with same name
//Get inode and get size

int sfs_getfilesize ( char* fname ) {
    int found = -1;
    int index = 0;
    for (int i = 0; i < 100; i++) {
        //Check if anyfile already in directory matches the filename
        if (directory_space[i] == 1) {
            found = strcmp(directory_cache[i].filename, fname);
            if (found == 0){
                index = i;
                break;
            }
        }
    }
    if (found != 0 ) {
       // printf("File doesn't exist, couldn't retrieve size\n");
        return -1;
    } else {
        int node_local = directory_cache[index].node_num;
        int result = inode_cache[node_local-1].size;
        return result;
    }
}

// Find the fd_entry in the fd_table
// Mark entry as 0 in the openfiles array
//Return 0 if
int sfs_fclose (int fd) {
    
    char* close_fd = (char *) calloc(sizeof(fd_entry),1);
    fd_entry * delete = (fd_entry *) close_fd;

    if (openfiles[fd] == 1){
        openfiles[fd] = 0;
        fd_table[fd] = *delete;
        return 0;
    } else {
       // printf("No open file in the fd location of fd_table\n");
        return -1;
    }
    
 
}

//----------------- WRITE TO FILE --------------------------------------------------

// Use fileID as index to locate fd_entry in fd_table[fileID]
// We then get the corresponding fd_entry and obtain inodenumber and rw pointer (ALWAYS SET AT THE BIT THAT IS READY TO BE WRITTEN TO);
// We then get inode from inodecache and then get last assigned pointer p[j] that is not 0
// We then do  1024-rw and check if the remaining data block is enough to write contents of cache
// IF NOT then:
//   1.- We want number of datablocks that we need to allocate = ceil(length - (1024-rw)/sizeofdatablock)
//   2.- Get free datablocks from disckblock_cache, if not enough display error message else proceed to writing to disk
//   3.- We write (1024-rw) bits from buffer into datablock
//   4.- We write remainder into disk. For last datablock update the rw pointer at the end of the remaining buffer length
//How do we write to remaining space in a datablock?
//We read contents of datablock into a char buffer [1024] and then add contents of new string to the buffer then re write to disk;

int sfs_fwrite(int fd, char* buffer, int length) {
        if (openfiles[fd] == 0 ) {
            return 0;
        }

        int rw = fd_table[fd].rw;
        int node_local = fd_table[fd].node_num;
        int end_of_file = inode_cache[node_local-1].size;
        int num_written = 0;
        
        if (end_of_file == 0) {

            int actual_size = 0;
            int num_datablocks = ceil(length/1024.0);

            for (int i = 0; i < num_datablocks; i++) {

                static char  buffer_inside [1024];
               
                int block_here;
                int principal = free_space();
                int suppl = free_inode();
                                            
               if (principal == -1 && suppl == -1) {
                   break;
               }
               else if (principal != -1){
                   block_here = principal;
                   diskblock_cache[block_here-102] = 1;
               }
                else {
                    block_here = suppl;
                    freeinodes[block_here-1] =1;
                }
               
                if (actual_size < 12) {

                    inode_cache[node_local-1].p[actual_size] = block_here;
                }
                //START ind_p structure
                else {
                    int indir =floor((actual_size-12)/92.0);
            
                    if (inode_cache[node_local-1].ind_p[indir] == 0) {
                       
                        int where_indir_array = free_indirect();
                        inode_cache[node_local-1].ind_p[indir] = where_indir_array;
                        indirect_array_space[where_indir_array] = 1;
                        indirect_array[where_indir_array][0] = block_here;

                        
                    }
                    else {
                        int temp = inode_cache[node_local-1].ind_p[indir];
                        for (int i = 0; i < 92; i ++) {
                            if (indirect_array[temp][i] == 0 ) {
                                indirect_array[temp][i] = block_here;
                                break;
                            }
                        }
                    }
                }
                // END ind_p structure

                for (int j = 0; j < 1024; j++) {
                
                     if ( ( (i*1024) +j) >= length) {
                          break;
                     }
                     num_written = num_written +1;
                     fd_table[fd].rw = fd_table[fd].rw +1;
                     inode_cache[node_local-1].size = inode_cache[node_local-1].size +1;
                     buffer_inside[j] = buffer[(i*1024) +j];
                }
                                     
                actual_size = actual_size +1;
                write_blocks(block_here,1, buffer_inside);
             
                                        
            } //End of for statement to write buffer into disk
            //Setting pointer to the end of file
                        
                 char *buffer_here = (char *) calloc(sizeof(inode),1);
                 inode* t = (inode*) buffer_here;
                 t->mode =  inode_cache[node_local-1].mode;
                 t->linkcnt =  inode_cache[node_local-1].linkcnt;
                 t->uid =  inode_cache[node_local-1].uid;
                 t->gid =  inode_cache[node_local-1].gid;
                 t->size =  inode_cache[node_local-1].size;
                                      
                 for (int i = 0; i < actual_size; i++) {
                      if (i < 12){
                         t->p[i] =  inode_cache[node_local-1].p[i];
                       } else {
                         t->ind_p[i-12] =  inode_cache[node_local-1].ind_p[i-12];
                       }
                 }
                  
            write_blocks(node_local,1, buffer_here);
            free(buffer_here);

            return num_written;
        } //END OF ELSE IF
        else {
            int datablock;
            int increased_by = 0;
            int which_b = 0;
            int which_p = 0;
            int last_modified = 0;
       
            for (int i = 0; i < length; i ++) {
                    
                    which_b = floor(rw/1024.0);
                    which_p = rw % 1024;
                    
                    if (which_b < 12) {
                        
                        if ( inode_cache[node_local-1].p[which_b] == 0) {
                            
                            int block_here;
                            int principal = free_space();
                            int suppl = free_inode();
                                                        
                           if (principal == -1 && suppl == -1) {
                               break;
                           }
                           else if (principal != -1){
                               block_here = principal;
                               diskblock_cache[block_here-102] = 1;
                           }
                            else {
                                block_here = suppl;
                                freeinodes[block_here-1] =1;
                            }
                            datablock = block_here;
                            inode_cache[node_local-1].p[which_b] = datablock;
                            diskblock_cache[datablock-102] = 1;
  
                        }
                        else {
                            datablock =  inode_cache[node_local-1].p[which_b];
                        }
                        last_modified = which_b;
                    }
                
                //START ind_p structure
                else {
                    int indir = floor((which_b - 12)/92.0);
                    int indir_pointer = (which_b - 12) % 92;
                   
                    if (inode_cache[node_local-1].ind_p[indir] == 0){
                        
                        int block_here;
                        int principal = free_space();
                        int suppl = free_inode();
                                                    
                       if (principal == -1 && suppl == -1) {
                           break;
                       }
                       else if (principal != -1){
                           block_here = principal;
                           diskblock_cache[block_here-102] = 1;
                       }
                        else {
                            block_here = suppl;
                            freeinodes[block_here-1] =1;
                        }
                        
                        int where_indir_array = free_indirect();
                        inode_cache[node_local-1].ind_p[indir] = where_indir_array;
                        indirect_array_space[where_indir_array] = 1;
                        indirect_array[where_indir_array][0] = block_here;
                        datablock = block_here;
                        
                    }
                    else {
       
                        int temp = inode_cache[node_local-1].ind_p[indir];
                        int temp_2 = indirect_array[temp][indir_pointer];
                        if (temp_2 == 0) {
                            
                            int block_here;
                            int principal = free_space();
                            int suppl = free_inode();
                                                        
                           if (principal == -1 && suppl == -1) {
                               break;
                           }
                           else if (principal != -1){
                               block_here = principal;
                               diskblock_cache[block_here-102] = 1;
                           }
                            else {
                                block_here = suppl;
                                freeinodes[block_here-1] =1;
                            }
                            indirect_array[temp][indir_pointer] = block_here;
                            datablock = block_here;
                            
                        }
                        else {
                            datablock = indirect_array[temp][indir_pointer];
                        }
                    }
                    last_modified = indir;
                }
                // END ind_p structure
       
                static char antes [1024];
                read_blocks(datablock,1,antes);
                antes[which_p] = buffer[i];
                write_blocks(datablock,1,antes);
                num_written = num_written +1;
                fd_table[fd].rw = fd_table[fd].rw + 1;
                rw = rw +1;
                if (rw>end_of_file){
                    increased_by = increased_by +1;
                    inode_cache[node_local-1].size = inode_cache[node_local-1].size +1;
                }
                

            } //END OF FOR LOOP
            //Update fd_table corresponding fd_entry
            //Update inode in cache and disk
 
            char *buffer_here = (char *) calloc(sizeof(inode),1);

            inode *t = (inode *) buffer_here;
            t->mode =  inode_cache[node_local-1].mode;
            t->linkcnt =  inode_cache[node_local-1].linkcnt;
            t->uid =  inode_cache[node_local-1].uid;
            t->gid =  inode_cache[node_local-1].gid;
            t->size =  inode_cache[node_local-1].size;
            
            for (int i = 0; i < 23 ; i++) {
                if (i < 12){
                    t->p[i] =  inode_cache[node_local-1].p[i];
                } else {
                    t->ind_p[i-12] =  inode_cache[node_local-1].ind_p[i-12];
                }
            }

            write_blocks(node_local,1, buffer_here);
            free(buffer_here);
            return num_written;
        
        } //END OF ELSE
        
} //END OF WRITE FUNCTION

int sfs_fread(int fd, char* buffer, int length) {

        if (openfiles[fd] == 0 ) {
            return 0;
        }
        int node_local = fd_table[fd].node_num;
        int rw = fd_table[fd].rw;
        int end_of_file = inode_cache[node_local-1].size;
        int num_read = 0;

    //If file is full we just display an error message
    if (rw == end_of_file) {
       // printf("Error: Can't read file you have reached end of file\n");
        return num_read;
    }

    else {
               for (int i = 0; i < length; i ++) {
                   
                   if (rw == end_of_file){
                       break;
                   }

                int which_b = floor(rw/1024.0);
                int which_p = rw % 1024;
                int datablock;

                if (which_b < 12) {
                    if ( inode_cache[node_local-1].p[which_b] == 0) {
                        break;
                    }
                    else {
                        datablock =  inode_cache[node_local-1].p[which_b];
                    }
                }
                   //START ind_p structure
                else {
                       int indir = floor((which_b-12)/92.0);
                       int indir_pointer = (which_b - 12) % 92;
                       if (inode_cache[node_local-1].ind_p[indir] == 0){
                           break;
                       }
                       else {
                        
                           int temp = inode_cache[node_local-1].ind_p[indir];
                           int temp_2 = indirect_array[temp][indir_pointer];
                           if (temp_2 == 0) {
                               break;
                               
                           }
                           else {
                               datablock = indirect_array[temp][indir_pointer];
                               
                           }
                       }
                   }
                   // END ind_p structure
                static char antes [1024];
                read_blocks(datablock,1,antes);
                buffer[i] = antes[which_p];
                num_read = num_read +1;
                fd_table[fd].rw = fd_table[fd].rw + 1;
                rw = rw +1;
                 
         
        } //END OF FOR LOOP
        //Update fd_table corresponding fd_entry
        //Update inode in cache and disk
        return num_read;
    
    } //END OF ELSE
    
}

int sfs_fseek(int fd, int position) {
    
    if (openfiles[fd] == 0 ) {
       // printf("No file open with fd entry %d\n", fd);
        return -1;
    }
    
    int node_local = fd_table[fd].node_num;
    int end_of_file = inode_cache[node_local-1].size;
    
    if ((position > end_of_file)||( position < 0)) {
        //printf("Invalid position for pointer\n");
        return -1;
    }

   fd_table[fd].rw = position;
   return 0;
    
    
}
//Delete datablocks associated with file
//To remove a file from system we delete entry in inode table both cache and disk
//Delete from fd_table
//Remove from directory table and cache

int sfs_remove(char* fname) {

    char * deletion = (char*)calloc(1024,1);
    char * deletion_inode = (char*)calloc(sizeof(inode),1);
    char * deletion_dir_entry = (char*)calloc(sizeof(dir_entry),1);
    char * deletion_fd_entry = (char*)calloc(sizeof(fd_entry),1);
    
    int found = -1;
    int index = 0;
    for (int i = 0; i < 100; i++) {
        //Check if anyfile already in directory matches the filename
        if (directory_space[i] == 1) {
            found = strcmp(directory_cache[i].filename, fname);
            if (found == 0){
                index = i;
                break;
            }
        }
    }
    //If file was not found then we can't remove it.
    if (found != 0 ) {
      //  printf("File doesn't exist, couldn't be removed from file system\n");
        return -1;
    }
    //If file was found then we can remove it.
    else {
        int node_local = directory_cache[index].node_num;
     
        //DELETING DATABLOCKS ASSIGNED TO FILE THROUGH INODE
        for (int i = 0 ; i < 23; i++) {
            int datablock;
            if(i < 12) {
                datablock =inode_cache[node_local-1].p[i];
            }
            else {
                datablock =inode_cache[node_local-1].ind_p[i-12];
            }
            if (datablock != 0) {
                diskblock_cache[datablock-102] = 0;
                write_blocks(datablock,1,deletion);
            }
        }// END OF FOR LOOP
        
        //DELETING INODE FROM inode_cache (MEMORY), inode_table (DISK), mark corresponding freeinodes entry as 0
    
        inode *h = (inode *)deletion_inode;
        inode_cache[node_local-1] = *h;
        write_blocks(node_local,1,deletion);
        freeinodes[node_local-1] = 0;
                
        //DELETING DIR_ENTRY FROM directory_cache (MEMORY), directory file datablock(DISK) and mark corresponsing directory_space entry as 0
        
        dir_entry *d = (dir_entry *)deletion_dir_entry;
        directory_cache[index] = *d;
        directory_space[index] = 0;
        
        int which_block = floor(index/42.0);
        int which_entry =index % 42;
        
        int blok = inode_cache[0].p[which_block];
        char before [1024];
        read_blocks (blok,1, before);
        //Adding directory entry to directory
        for (int i = 0; i < 24; i++) {
            before[(which_entry*24)+i] = deletion_dir_entry[i];
        }
        write_blocks(blok,1,before);
        //DELETING FD_ENTRY FROM FD_TABLE(Memory), mark corresponding openfiles entry as 0
        //CHECK IF FILE IS OPEN FIRST
        
        int is_open = -1;
        int index = -1;
        
        for (int i = 0; i < 100;i++) {
            if (openfiles[i] == 1) {
                if(fd_table[i].node_num == node_local){
                    is_open = 1;
                    index = i;
                }
            }
        }
        if(is_open == 1){
            fd_entry *del = (fd_entry *) deletion_fd_entry;
            fd_table[index] = *del;
            openfiles[index] = 0;
        }
 
        return 0;

    } //END OF ELSE STATEMENT
   
}
