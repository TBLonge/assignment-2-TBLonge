#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"

int mounted=0;

int mdadm_mount(void) {
    if (mounted==1) {
        return -1;  // Fail if already mounted
    }
    uint32_t op = (JBOD_MOUNT << 12);
    int result = jbod_operation(op, NULL);
    if (result == 0) {
        mounted = 1;
        return 1;
    }
    return -1;
}

int mdadm_unmount(void) {
    if (mounted==0) {
        return -1;  // Fail if already unmounted
    }
    uint32_t op = (JBOD_UNMOUNT << 12);
    int result = jbod_operation(op, NULL);
    if (result == 0) {
        mounted = 0;
        return 1;
    }
    return -1;
}


int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {
    // Check if the system is mounted
    if (mounted == 0) {
        return -3;  // System not mounted
    }

    // Check for invalid parameters
    if (start_addr + read_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS) {
        return -1;  // Out of bounds
    }
    if (read_len > 1024) {
        return -2;  // Read length exceeds limit
    }
    if (read_len > 0 && read_buf == NULL) {
        return -4;  // Invalid buffer
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
