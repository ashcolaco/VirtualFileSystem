#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_disk.h"
#include "jfs_common.h"

#define MAGICNUM 0x89abcdef


/*
*   This function copies all the blocks stored in the log file up until
*   the uncommit block, to the filesystem disk at the block locations 
*   stored in the commit block.
*/
void copy_log_to_disk(jfs_t *jfs, struct inode i_node, struct commit_block* cb, int limit){
    
    for (int blocks_done = 0; blocks_done<INODE_BLOCK_PTRS; blocks_done++) {
        char block[BLOCKSIZE];
        
        //Gather the log block number and the blocknum at the commitblock
        int lognum = i_node.blockptrs[blocks_done];
        int blocknum = cb->blocknums[blocks_done];
        
        //Stop right at the uncommit block
        if (blocknum < 0 || limit == blocks_done) {
            break;
        }
        
        //Read the block to copy in the log file
        jfs_read_block(jfs, block, lognum);
        
        //Write that block to the disk
        write_block(jfs->d_img, block, blocknum);
        
    }
 
}


/*
*   This function checks if the log file exist. It then proceeds to go 
*   through the log file's inode blocks to find a commit block found 
*   uncommited.  Once its found the block to fix the filesystem, it then 
*   overwrite the uncommit block as a commit block.
*/
void _checklog(jfs_t *jfs) {
    struct commit_block* cb;
    struct inode i_node;
    char block[BLOCKSIZE];

    int root_inodenum = find_root_directory(jfs);

    //Check if the file exist to proceed first
    int inodenum = findfile_recursive(jfs, ".log", root_inodenum, DT_FILE);
    if (inodenum < 0) {
        printf("Fatal Error: .log file not found.\n");
        exit(1);
    }


    //Get inode of the .log file
	get_inode(jfs, inodenum, &i_node);
    

    //Iterate through the inode block pointers
    for (int i = 0; i < INODE_BLOCK_PTRS; i++) {

        int blocknum = i_node.blockptrs[i];
        
        //Read out the commit block
        jfs_read_block(jfs,block, blocknum);
        cb = (struct commit_block *)block;
        
        
        //Proceed if there exist a uncommited block recorded
        if (cb->magicnum == MAGICNUM && cb->uncommitted == 1) {

            //Finish copying the log block to the disk block
            copy_log_to_disk(jfs, i_node, cb, i);
        
            
            //Clear the uncommit block
            for (int i = 0; i < INODE_BLOCK_PTRS; i++) {
                cb->blocknums[i] = -1;
            }
            cb->sum = 0;
            cb->uncommitted = 0;
            
            //Overwrite it as a commit block
            write_block(jfs->d_img, block, blocknum);
        }
    }

}


/*  
*   A program that restores the filesystem back to a consistent state,
*   by looking through the log file to find any non fully committed
*   block, and corrects it by copying old data found in the log file
*   to where it should be in the disk.
*/
int main(int argc, char **argv) {
    struct disk_image *di;
    jfs_t *jfs;

    if (argc != 2) {
        printf("Usage:  jfs_checklog <volumename>\n");
        exit(1);
    }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
    
    
    _checklog(jfs);
    
    
    unmount_disk_image(di);
    exit(0);
}
