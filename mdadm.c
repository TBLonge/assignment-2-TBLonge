#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"

int mounted=0;

int mdadm_mount(void) {
    //check to see if system is mounted already 
    if(mounted == 1) {
        return -1; 
    }
    uint32_t op = (jbod_unmount);
    int state = jbod_operation(op, NULL);
    if(state == 0){
        op();
        mounted = 1;
        return 1;
    }
    return -1;
}

int mdadm_unmount(void) {
    //check to see if system is already unmounted 
    if(mounted == 0){
        return -1; //Fail
    }
    uint32_t op = (jbod_unmount);
    int state = jbod_operation(op, NULL);
    if(mounted == 1){
        op();
        return 1;
    }
    return -1;
}

int jbod_sequential_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {
    uint32_t bytes = 0;
    while(bytes < JBOD_DISK_SIZE*NUMBER_OF_DISKS){
        if(start_addr + bytes >= JBOD_DISK_SIZE*NUMBER_OF_DISKS){
            break;
        }
        bytes += 256
    }
    return bytes;
}
//1. Jbod_sequential_read would be a helper function for mdadm_read, it would use the exsisting format of mdadm_read while iterating through each block until it reaches the maximum disk size.  
//2. More effecient since mdadm_read won't be called multiple times.
//3. It would be helpful for any problems that involve iterating through a large data set. 

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {
    //Check is system is mounted
    if(mounted != 1){
        return -3;
    }

    if(read_len > 1024){
        return -2;
    }

    if(start_addr + read_len > JBOD_DISK_SIZE*NUMBER_OF_DISKS){
        return -1;
    }
    if(read_len > 0 && read_buf == NULL){
        return -4; 
    }
    
    

    uint32_t bytes_read = 0;
    uint32_t current_addr = start_addr;

    while (bytes_read < read_len) {
        uint32_t disk_id = current_addr / JBOD_DISK_SIZE;  // Determine the disk number
        uint32_t block_id = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;  // Determine the block number
        uint32_t offset = (current_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE; // Offset within the block
       
        // Seek to the correct disk
        uint32_t op = (JBOD_SEEK_TO_DISK << 12) | disk_id;
        if (jbod_operation(op, NULL) == -1) {
            return -1;  // Error seeking to disk
        }

        // Seek to the correct block
        op = (JBOD_SEEK_TO_BLOCK << 12) | (block_id & 0xFF)<<4;  // Ensure block_id fits in the relevant bits
        if (jbod_operation(op, NULL) == -1) {
            return -1;  // Error seeking to block
        }

        // Read the block
        uint8_t block[JBOD_BLOCK_SIZE];  // Buffer to hold the block data
        op = (JBOD_READ_BLOCK << 12);
        if (jbod_operation(op, block) == -1) {
            return -1;  // Error reading block
        }

        // Calculate how many bytes to copy from the block
        uint32_t bytes_to_copy = JBOD_BLOCK_SIZE - offset;  // Calculate how many bytes can be read from the current block
        if (bytes_to_copy > read_len - bytes_read) {  // Do not exceed remaining bytes to read
            bytes_to_copy = read_len - bytes_read; 
        }
        
        // Copy the relevant portion of the block to the output buffer
        memcpy(read_buf + bytes_read, block + offset, bytes_to_copy);  // Copy data to the read buffer

        // Update read progress
        bytes_read += bytes_to_copy;  // Increase total bytes read
        current_addr += bytes_to_copy;  // Move the current address forward
    }

    return bytes_read;  // Return the total number of bytes read
}
