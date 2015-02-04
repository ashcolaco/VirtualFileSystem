#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "fs_disk.h"
#include "jfs_common.h"


/*
*   This function clears all the blocks pointers held by the inode
*   returns it to the freelist.
*/
void rm_file(jfs_t *jfs, int inodenum) {
    struct inode i_node;
    
    get_inode(jfs, inodenum, &i_node);
    
    //Calculate the number of complete blocks
    int completeblocks = (i_node.size + BLOCKSIZE - 1)/ BLOCKSIZE;

    //Iterate to free all the complete blocks
    for (int i = 0; i < completeblocks; i++) {
    
        return_block_to_freelist(jfs, i_node.blockptrs[i]);
        
    }
    
    //Send file's inode into freelist
    return_inode_to_freelist(jfs, inodenum);
    
}


/*
*   This function searches for the file directory entry and removes
*   it by overriding its place in the block with the subsequent bytes 
*   after the file.
*/
int rm_dirent(jfs_t *jfs, int blocknum, char *filename) {
    struct dirent *d_ent;
    char block[BLOCKSIZE];
    char name[MAX_FILENAME_LEN + 1];
    int bytes_done = 0;
 
    jfs_read_block(jfs, block, blocknum);
     
    //Iterate through the parent directory to find the file directory
    while (1) {
        d_ent = (struct dirent*)(block+bytes_done);
        memcpy(name, d_ent->name, d_ent->namelen);
        name[d_ent->namelen] = '\0';
 
        if (strcmp(name,filename) == 0) {
            break;
        }
        bytes_done += d_ent->entry_len;
    }
 
    //Temporarily hold this file size
    int size = d_ent->entry_len;
     
 
    char new_block[BLOCKSIZE];
     
    //Slice the subblock before the file
    memcpy(new_block, block, bytes_done);
 
    int offset = d_ent->entry_len+bytes_done;
    int bytes_left = BLOCKSIZE-offset;
 
    //Slice and append the subblock after the file
    memcpy(new_block+bytes_done, block+offset, bytes_left);
 
    //Save this new block as replacement
    jfs_write_block(jfs, new_block, blocknum);
     
     
    //Return the directory entry's previously held size for update
    return size;
 
}


/*
*   This function checks if the given filename in the path exists in 
*   the filesystem, before attempting to delete it.
*/
void _rm(jfs_t *jfs, char *pathname) {
    struct inode *dir_inode;
    char dirname[MAX_FILENAME_LEN], filename[MAX_FILENAME_LEN];
    char block[BLOCKSIZE];
     
  
    int root_inodenum = find_root_directory(jfs);
    while(pathname[0]=='/') {
        pathname++;
    }
 
    //Check if the file exist to proceed first
    int file_inodenum = findfile_recursive(jfs, pathname, root_inodenum, DT_FILE);
    if (file_inodenum < 0) {
        printf("Error: %s is not a valid file path\n", pathname);
        exit(1);
    }
 
    //Check if file is ok to remove
    last_part(pathname, filename);
    if (strcmp(filename,".log") == 0) {
        printf("Illegal: .log cannot be deleted.\n");
        exit(1);
    }
    
    //Get the file's parent inode number
    all_but_last_part(pathname, dirname);
    int dir_inodenum = root_inodenum;
    if (strlen(dirname) != 0) {
        dir_inodenum = findfile_recursive(jfs, dirname, root_inodenum, DT_DIRECTORY);
    }
     
    //Get the directories inode by reading its block
    jfs_read_block(jfs, block, inode_to_block(dir_inodenum));
    dir_inode = (struct inode*)(block + (dir_inodenum % INODES_PER_BLOCK) * INODE_SIZE);
    
     
    //Remove the file and its directory entry
    int dirent_size = rm_dirent(jfs, dir_inode->blockptrs[0], filename);
    rm_file(jfs, file_inodenum);
     
     
    //Update the inode with the reduced size and write the block
    dir_inode->size -= dirent_size;
    jfs_write_block(jfs, block, inode_to_block(dir_inodenum));
 
 
    //Commit the changes
    jfs_commit(jfs);
 
}


/*  
*   A program that remove a file from any directory (including the root directory) 
*   on the virtual filesystem, leaving the filesystem in a consistent state
*/
int main(int argc, char **argv) {
    struct disk_image *di;
    jfs_t *jfs;

    if (argc != 3) {
        printf("Usage:  jfs_rm <volumename> <filename>\n");
        exit(1);
    }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
    
    
    _rm(jfs, argv[2]);
    
    
    unmount_disk_image(di);
    exit(0);
}