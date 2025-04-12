#include "filesys.h"

#include <string.h>
#include <assert.h>

#include "utility.h"
#include "debug.h"

#define INDIRECT_DBLOCK_INDEX_COUNT (DATA_BLOCK_SIZE / sizeof(dblock_index_t) - 1)
#define INDIRECT_DBLOCK_MAX_DATA_SIZE ( DATA_BLOCK_SIZE * INDIRECT_DBLOCK_INDEX_COUNT )

#define NEXT_INDIRECT_INDEX_OFFSET (DATA_BLOCK_SIZE - sizeof(dblock_index_t))

// ----------------------- UTILITY FUNCTION ----------------------- //

// Debug Functions
void print_inode_data(filesystem_t *fs, inode_t *inode) {
    if (fs == NULL || inode == NULL) {
        fprintf(stderr, "Invalid input: filesystem or inode is NULL.\n");
        return;
    }

    size_t file_size = inode->internal.file_size;
    if (file_size == 0) {
        printf("The inode has no data.\n");
        return;
    }

    printf("Data in inode (size: %lu bytes):\n", file_size);

    // Read from direct D-blocks
    for (size_t i = 0; i < INODE_DIRECT_BLOCK_COUNT; i++) {
        if (inode->internal.direct_data[i] == 0) break; // No more direct D-blocks

        size_t block_index = inode->internal.direct_data[i];
        char *block_data = (char *)&fs->dblocks[block_index * 64];

        // Calculate how much to read from this block
        size_t bytes_to_read = (file_size > 64) ? 64 : file_size;

        // Print the data as characters
        for (size_t j = 0; j < bytes_to_read; j++) {
            printf("%c", block_data[j]);
        }

        file_size -= bytes_to_read;
        if (file_size == 0) break; // All data has been read
    }

    // Handle indirect D-blocks if there is remaining data
    if (file_size > 0 && inode->internal.indirect_dblock != 0) {
        size_t indirect_block_index = inode->internal.indirect_dblock;
        dblock_index_t *indirect_block = (dblock_index_t *)&fs->dblocks[indirect_block_index * 64];

        for (size_t i = 0; i < 15 && file_size > 0; i++) {
            if (indirect_block[i] == 0) break; // No more indirect D-blocks

            size_t block_index = indirect_block[i];
            char *block_data = (char *)&fs->dblocks[block_index * 64];

            // Calculate how much to read from this block
            size_t bytes_to_read = (file_size > 64) ? 64 : file_size;

            // Print the data as characters
            for (size_t j = 0; j < bytes_to_read; j++) {
                printf("%c", block_data[j]);
            }

            file_size -= bytes_to_read;
        }
    }

    printf("\n");
}

void print_dblock_data(filesystem_t *fs, dblock_index_t dblock_index) {
    if (fs == NULL) {
        fprintf(stderr, "Error: Filesystem is NULL.\n");
        return;
    }

    // Ensure the D-block index is within range
    if (dblock_index >= fs->dblock_count) {
        fprintf(stderr, "Error: D-block index %u is out of range (max: %lu).\n", 
                dblock_index, fs->dblock_count - 1);
        return;
    }

    // Get a pointer to the start of the D-block
    char *dblock_start = (char *)&fs->dblocks[dblock_index * 64];

    // Print the contents of the D-block as characters
    printf("Contents of D-block %u:\n", dblock_index);
    for (size_t i = 0; i < 64; i++) {
        char c = dblock_start[i];
        if (c >= 32 && c <= 126) { // Printable ASCII range
            printf("%c", c);
        } else {
            printf("."); // Non-printable characters are shown as '.'
        }
    }
    printf("\n");
}

void free_indirect(filesystem_t *fs, inode_t *inode, size_t offset){
    size_t size_of_indirect = inode->internal.file_size-256;
    size_t data_dblocks_of_indirect = (size_of_indirect)/64;

    size_t offset_dblocks = ((offset+63)/64);

    size_t consider_offset = 0;

    if (offset>0){
        consider_offset = 1;
    }
    

    dblock_index_t cur_index = inode->internal.indirect_dblock;

    size_t data_dblocks_read = 0;

    size_t is_first = 0;

    // free all indirect index dblock first
    for (size_t i = 0; i < calculate_index_dblock_amount(size_of_indirect); i++){
        for (size_t i = 0; i<15 && data_dblocks_read<data_dblocks_of_indirect; i++){
            if (data_dblocks_read < offset_dblocks) {
                data_dblocks_read++;
                continue;
            }
            dblock_index_t cur_dblock_idx;
            memcpy(&cur_dblock_idx,&fs->dblocks[cur_index*64]+i*4,4);
            release_dblock(fs,&fs->dblocks[cur_dblock_idx*64]);
            data_dblocks_read++;
        }

        if (consider_offset == 0){
            release_dblock(fs,&fs->dblocks[cur_index*64]);       
        } else {
            if (is_first == 0){
                is_first = 1;
            } else {
                release_dblock(fs,&fs->dblocks[cur_index*64]);  
            }
        }
        memcpy(&cur_index,&fs->dblocks[cur_index*64]+60,4);
    }
}

// ----------------------- CORE FUNCTION ----------------------- //

void write_to_dblock(filesystem_t *fs, dblock_index_t dblock_idx, size_t offset_val, void *data, size_t n){
    memcpy(&fs->dblocks[(dblock_idx*64)+offset_val],data,n);
}

fs_retcode_t inode_write_data(filesystem_t *fs, inode_t *inode, void *data, size_t n)
{
    //Check for valid input
    if (fs == NULL || inode == NULL){return INVALID_INPUT;}
    if (n == 0){return SUCCESS;} 
    if (available_dblocks(fs) < calculate_necessary_dblock_amount(n)) {
        return INSUFFICIENT_DBLOCKS;
    }

    // If inode is empty
    if (inode->internal.file_size == 0){
        size_t blocks_needed = calculate_necessary_dblock_amount(n);
        if (available_dblocks(fs) < blocks_needed) {
            return INSUFFICIENT_DBLOCKS;
        }
        
        // Write data to the direct dblocks
        for (int i = 0; i < 4 && n > 0; i++){
            dblock_index_t temp_dblock;
            if (claim_available_dblock(fs, &temp_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;
            if (n>=64){
                write_to_dblock(fs,temp_dblock,0,data,64);
                inode->internal.file_size += 64;
                data = (void *)((byte *)data + 64);
                n = (n - 64);
            } else {
                write_to_dblock(fs,temp_dblock,0,data,n);
                inode->internal.file_size += n;
                data = (void *)((byte *)data + n);
                n = 0;
            }
            inode->internal.direct_data[i] = temp_dblock; // Add dblock index value
        }

        // Check to see if data left
        if (n==0) {return SUCCESS;}

        // Write Remaining Data to Indirect Dblock
        size_t dblocks_needed = calculate_necessary_dblock_amount(n); // (idx dblock + data dblock)
        size_t idx_dblocks_needed = calculate_index_dblock_amount(n); // (idx dblock)
        size_t data_dblocks_needed = (dblocks_needed-idx_dblocks_needed);

        dblock_index_t temp_previous_idx_dblock;

        for (size_t i = 0; i < idx_dblocks_needed && n > 0; i++){
            dblock_index_t temp_idx_dblock;
            if (claim_available_dblock(fs, &temp_idx_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;

            if (i==0) inode->internal.indirect_dblock = temp_idx_dblock;
            else write_to_dblock(fs, temp_previous_idx_dblock, 60, &temp_idx_dblock, 4);

            for (int j = 0; n > 0 && j < 15; j++){
                dblock_index_t temp_dblock;
                if (claim_available_dblock(fs, &temp_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;
                if (n>=64){
                    write_to_dblock(fs,temp_dblock,0,data,64);
                    inode->internal.file_size += 64;
                    data = (void *)((byte *)data + 64);
                    n = (n - 64);
                } else {
                    write_to_dblock(fs,temp_dblock,0,data,n);
                    inode->internal.file_size += n;
                    data = (void *)((byte *)data + n);
                    n = 0;
                }

                // Storing the index value of data dblock inside index dblock
                write_to_dblock(fs, temp_idx_dblock, j*4, &temp_dblock, 4);
                data_dblocks_needed--;
            }

            temp_previous_idx_dblock = temp_idx_dblock;
        }
    } else {
        size_t byte_in_last_data_dblock = (inode->internal.file_size%64);
        if (byte_in_last_data_dblock == 0) byte_in_last_data_dblock = 64;
        size_t data_dblocks_in_inode = ((inode->internal.file_size+63)/64);

        // Is direct dblock being used
        if (data_dblocks_in_inode<=4){
            size_t space_left_last_data_dblock = 64-byte_in_last_data_dblock;
            dblock_index_t last_data_dblock = inode->internal.direct_data[data_dblocks_in_inode-1];
            
            write_to_dblock(fs,last_data_dblock,byte_in_last_data_dblock,data,space_left_last_data_dblock);

            inode->internal.file_size += space_left_last_data_dblock;
            data = (void *)((byte *)data + space_left_last_data_dblock);
            n = (n - space_left_last_data_dblock);

            // BEGINNING OF THE COPY PASTE (im so sorry)
            size_t blocks_needed = calculate_necessary_dblock_amount(n);
            if (available_dblocks(fs) < blocks_needed) {
                return INSUFFICIENT_DBLOCKS;
            }
            
            // Check to see if data left
            if (n==0) {return SUCCESS;}
            
            // Write data to the direct dblocks
            for (int i = data_dblocks_in_inode; i < 4 && n > 0; i++){
                dblock_index_t temp_dblock;
                if (claim_available_dblock(fs, &temp_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;
                if (n>=64){
                    write_to_dblock(fs,temp_dblock,0,data,64);
                    inode->internal.file_size += 64;
                    data = (void *)((byte *)data + 64);
                    n = (n - 64);
                } else {
                    write_to_dblock(fs,temp_dblock,0,data,n);
                    inode->internal.file_size += n;
                    data = (void *)((byte *)data + n);
                    n = 0;
                }
                inode->internal.direct_data[i] = temp_dblock; // Add dblock index value
            }

            // Check to see if data left
            if (n==0) {return SUCCESS;}

            dblock_index_t temp_previous_idx_dblock;

            for (size_t i = 0; n > 0; i++){
                dblock_index_t temp_idx_dblock;
                if (claim_available_dblock(fs, &temp_idx_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;

                if (i==0) inode->internal.indirect_dblock = temp_idx_dblock;
                else write_to_dblock(fs, temp_previous_idx_dblock, 60, &temp_idx_dblock, 4);

                for (int j = 0; n > 0 && j < 15; j++){
                    dblock_index_t temp_dblock;
                    if (claim_available_dblock(fs, &temp_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;
                    if (n>=64){
                        write_to_dblock(fs,temp_dblock,0,data,64);
                        inode->internal.file_size += 64;
                        data = (void *)((byte *)data + 64);
                        n = (n - 64);
                    } else {
                        write_to_dblock(fs,temp_dblock,0,data,n);
                        inode->internal.file_size += n;
                        data = (void *)((byte *)data + n);
                        n = 0;
                    }

                    // Storing the index value of data dblock inside index dblock
                    write_to_dblock(fs, temp_idx_dblock, j*4, &temp_dblock, 4);
                }

                temp_previous_idx_dblock = temp_idx_dblock;
            }
        } else { // If all the data in Indirect data block
            size_t indirect_data_dblocks_in_inode = (data_dblocks_in_inode-4);
            size_t indirect_idx_dblocks_in_inode;
            
            if (byte_in_last_data_dblock == 64) indirect_idx_dblocks_in_inode = calculate_index_dblock_amount(indirect_data_dblocks_in_inode*64);
            else indirect_idx_dblocks_in_inode = calculate_index_dblock_amount((indirect_data_dblocks_in_inode-1)*64+byte_in_last_data_dblock);

            dblock_index_t index_for_last_idx_dblock = inode->internal.indirect_dblock;
            
            for (size_t i = 0; i < indirect_idx_dblocks_in_inode-1; i++){
                memcpy(&index_for_last_idx_dblock, &fs->dblocks[index_for_last_idx_dblock*64+60], 4);
            }

            size_t indirect_data_dblocks_last_index_node = (indirect_data_dblocks_in_inode%15);
            dblock_index_t index_for_indirect_data_dblocks_last_index_node;
            
            if (indirect_data_dblocks_last_index_node != 0){
                memcpy(&index_for_indirect_data_dblocks_last_index_node, &fs->dblocks[index_for_last_idx_dblock*64+((indirect_data_dblocks_last_index_node-1)*4)], 4);
                write_to_dblock(fs,index_for_indirect_data_dblocks_last_index_node,byte_in_last_data_dblock,data,64-byte_in_last_data_dblock);
                data = (void *)((byte *)data + (64-byte_in_last_data_dblock));
                n = (n - (64-byte_in_last_data_dblock));
                inode->internal.file_size += (64-byte_in_last_data_dblock);
                
                // Fill up remaining datablocks in the index dblock
                for (int j = indirect_data_dblocks_last_index_node; j < 15; j++){
                    dblock_index_t temp_dblock;
                    if (claim_available_dblock(fs, &temp_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;
                    if (n>=64){
                        write_to_dblock(fs,temp_dblock,0,data,64);
                        inode->internal.file_size += 64;
                        data = (void *)((byte *)data + 64);
                        n = (n - 64);
                    } else {
                        write_to_dblock(fs,temp_dblock,0,data,n);
                        inode->internal.file_size += n;
                        data = (void *)((byte *)data + n);
                        n = 0;
                    }
                    write_to_dblock(fs, index_for_last_idx_dblock, j*4, &temp_dblock, 4);
                }
            }     

            // Check to see if data left
            if (n==0) {return SUCCESS;}

            dblock_index_t temp_previous_idx_dblock = index_for_last_idx_dblock;

            for (size_t i = 0; n > 0; i++){
                dblock_index_t temp_idx_dblock;
                if (claim_available_dblock(fs, &temp_idx_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;

                write_to_dblock(fs, temp_previous_idx_dblock, 60, &temp_idx_dblock, 4);

                for (int j = 0; n > 0 && j < 15; j++){
                    dblock_index_t temp_dblock;
                    if (claim_available_dblock(fs, &temp_dblock) != SUCCESS) return INSUFFICIENT_DBLOCKS;
                    if (n>=64){
                        write_to_dblock(fs,temp_dblock,0,data,64);
                        inode->internal.file_size += 64;
                        data = (void *)((byte *)data + 64);
                        n = (n - 64);
                    } else {
                        write_to_dblock(fs,temp_dblock,0,data,n);
                        inode->internal.file_size += n;
                        data = (void *)((byte *)data + n);
                        n = 0;
                    }

                    // Storing the index value of data dblock inside index dblock
                    write_to_dblock(fs, temp_idx_dblock, j*4, &temp_dblock, 4);
                }

                temp_previous_idx_dblock = temp_idx_dblock;
            }
        }
    }

    return SUCCESS;
}

fs_retcode_t inode_read_data(filesystem_t *fs, inode_t *inode, size_t offset, void *buffer, size_t n, size_t *bytes_read)
{   
    // Check inputs
    if (fs == NULL || inode == NULL || buffer == NULL || bytes_read == NULL) 
        return INVALID_INPUT;
    
    size_t size_of_inode = inode->internal.file_size;
    
    // If offset is beyond file or file is empty, read 0 bytes
    if (offset >= size_of_inode || size_of_inode == 0) {
        return SUCCESS;
    }

    *bytes_read = 0;
    
    // Calculate how many bytes we can actually read
    size_t bytes_to_read = n;
    if (offset + bytes_to_read > size_of_inode) {
        bytes_to_read = size_of_inode - offset;
    }

    size_t bytes_to_read_direct_dblock = bytes_to_read;
    size_t bytes_to_read_indirect_dblock = bytes_to_read;
    
    // Read all direct blocks into buffer
    if (offset<256){
        byte *entire_direct_dblock = malloc(64 * 4);
        size_t last_data_block_index = (size_of_inode - 1) / 64;
        for (size_t i = 0; i <= last_data_block_index && i < 4; ++i) {
            memcpy(entire_direct_dblock + (i * 64), &fs->dblocks[inode->internal.direct_data[i] * 64], 64);
        }

        if (bytes_to_read + offset > 256) {
            bytes_to_read_indirect_dblock = (bytes_to_read + offset) - 256;
            bytes_to_read_direct_dblock = bytes_to_read - bytes_to_read_indirect_dblock;
            offset = 0;
        }

        memcpy(buffer, entire_direct_dblock + offset, bytes_to_read_direct_dblock);
        *bytes_read += bytes_to_read_direct_dblock;
        free(entire_direct_dblock);

    } else {
        offset = offset - 256;
        bytes_to_read_indirect_dblock = n;
    }

    if (size_of_inode<=256 || bytes_to_read == *bytes_read){
        // No Indirect Block, quit program
        return SUCCESS;
    }

    // If: offset is at indirect dblock, 
    // Then: offset = some non zero value, bytes_to_read_indirect_block

    size_t offset_idx_dblock = calculate_index_dblock_amount(offset);
    size_t offset_data_dblocks_to_read = calculate_necessary_dblock_amount(offset) - offset_idx_dblock;
    size_t offset_byte_block = (offset%64);
    size_t bytes_last_indirect_dblock = (bytes_to_read_indirect_dblock%64);

    dblock_index_t idx_next_indirect_dblock = inode->internal.indirect_dblock;
    size_t last_idx_dblock = calculate_index_dblock_amount(bytes_to_read_indirect_dblock);
    size_t data_dblocks_to_read = calculate_necessary_dblock_amount(bytes_to_read_indirect_dblock) - last_idx_dblock;

    size_t total_dblock_counter = 0;

    for (size_t i = 0; i <= last_idx_dblock; i++){

        size_t dblock_counter = 0;

        while (dblock_counter<15 && data_dblocks_to_read>0){

            if (total_dblock_counter < offset_data_dblocks_to_read){
                dblock_counter++;
                total_dblock_counter++;
                continue;
            }

            dblock_index_t index_for_data_dblock;
            memcpy(&index_for_data_dblock, &fs->dblocks[idx_next_indirect_dblock*64] + 4*dblock_counter, 4);

            if (data_dblocks_to_read != 1){
                memcpy(buffer+*bytes_read,&fs->dblocks[index_for_data_dblock*64 + offset_byte_block],64);
                *bytes_read += 64;
            } else {
                memcpy(buffer+*bytes_read,&fs->dblocks[index_for_data_dblock*64 + offset_byte_block],bytes_last_indirect_dblock);
                *bytes_read += bytes_last_indirect_dblock;
            }
            offset_byte_block = 0;
            data_dblocks_to_read--;
            dblock_counter++;
            total_dblock_counter++;
        }
        memcpy(&idx_next_indirect_dblock, &fs->dblocks[idx_next_indirect_dblock*64] + 60, 4);
    }
    return SUCCESS;
}

fs_retcode_t inode_modify_data(filesystem_t *fs, inode_t *inode, size_t offset, void *buffer, size_t n)
{   
    if (fs == NULL || inode == NULL) return INVALID_INPUT;
    if (offset > inode->internal.file_size) return INVALID_INPUT;

    //calculate the final filesize and verify there are enough blocks to support it
    size_t file_size = inode->internal.file_size;
    size_t upper_bound = offset+n; // Exclusive

    if (((upper_bound-file_size)/64 > available_dblocks(fs)) && file_size<256) return INSUFFICIENT_DBLOCKS; // When assigning blocks to direct block


    size_t indirect_bytes = upper_bound-256;
    size_t indirect_file_size = file_size-256;

    size_t ib_index_blocks = calculate_index_dblock_amount(indirect_bytes);
    size_t ifz_index_blocks = calculate_index_dblock_amount(indirect_file_size);

    if (ifz_index_blocks<ib_index_blocks){
        if ((ib_index_blocks-ifz_index_blocks)>available_dblocks(fs)) return INSUFFICIENT_DBLOCKS;
    }

    if (offset > file_size){ // If offset is larger, just append data
        inode_write_data(fs,inode,buffer,n);
        return SUCCESS;
    };

    size_t buffer_written = 0;

    // Manage Modifying just the Direct Datablock
    if (offset<256 && upper_bound<256){
        if (upper_bound > file_size){
            upper_bound = file_size;
        }
        size_t lower_offset_block_index = offset / 64;
        size_t upper_offset_block_index = (upper_bound - 1) / 64;  // upper_bound is exclusive
        
        size_t bytes_in_lower_offset_block = offset % 64;          // Starting byte in first block
        size_t bytes_in_upper_offset_block = (upper_bound - 1) % 64 + 1;  // Bytes to write in last block        

        for (size_t i = lower_offset_block_index; i <= upper_offset_block_index; i++){

            if (i == upper_offset_block_index && i == lower_offset_block_index ){
                memcpy(&fs->dblocks[(inode->internal.direct_data[i])*64]+bytes_in_lower_offset_block,buffer+buffer_written,upper_bound-offset);
                buffer_written += (upper_bound-offset);
                continue;
            }

            if (i == lower_offset_block_index){
                memcpy(&fs->dblocks[(inode->internal.direct_data[i])*64]+bytes_in_lower_offset_block,buffer+buffer_written,(64-bytes_in_lower_offset_block));
                buffer_written += (64-bytes_in_lower_offset_block);
                continue;
            }

            if (i == upper_offset_block_index){
                memcpy(&fs->dblocks[(inode->internal.direct_data[i])*64],buffer+buffer_written,bytes_in_upper_offset_block);
                buffer_written += bytes_in_upper_offset_block;
                continue;
            }

            memcpy(&fs->dblocks[(inode->internal.direct_data[i])*64],buffer+buffer_written,64);
            buffer_written += 64;
        }
    } else if (offset<256 && upper_bound>=256){
        
        size_t indirect_bytes = upper_bound-256;

        size_t lower_offset_block_index = offset / 64;
        size_t bytes_in_lower_offset_block = offset % 64;    

        for (size_t i = lower_offset_block_index; i <= 3; i++){
            memcpy(&fs->dblocks[(inode->internal.direct_data[i])*64]+bytes_in_lower_offset_block,buffer+buffer_written,(64-bytes_in_lower_offset_block));
            buffer_written += (64-bytes_in_lower_offset_block);
            bytes_in_lower_offset_block = 0;    
        }

        if (file_size == 256){
            inode_write_data(fs,inode,buffer,((offset+n)-256));
            return SUCCESS;
        }

        if (indirect_bytes > (file_size-256)){
            indirect_bytes = file_size;
        }

        size_t upper_bound_indirect_data_block = (indirect_bytes - 1) / 64; 
        size_t upper_bound_bytes = (indirect_bytes - 1) % 64 + 1;

        size_t data_dblocks_read = 0;
        size_t data_dblocks_per_idx_read = 0;

        dblock_index_t cur_index = inode->internal.indirect_dblock;

        while (data_dblocks_read <= upper_bound_indirect_data_block){
            size_t idx_of_data_dblock = 0;
            memcpy(&idx_of_data_dblock,&fs->dblocks[cur_index*64]+data_dblocks_per_idx_read*4,4);

            if (data_dblocks_read == upper_bound_indirect_data_block){
                memcpy(&fs->dblocks[idx_of_data_dblock*64],buffer+buffer_written,upper_bound_bytes);
                buffer_written += upper_bound_bytes;
                data_dblocks_read++;
                data_dblocks_per_idx_read++;
                continue;
            }

            if (data_dblocks_read < (upper_bound_indirect_data_block)){
                memcpy(&fs->dblocks[idx_of_data_dblock*64],buffer+buffer_written,64);
                buffer_written += 64;
            }

            data_dblocks_read++;
            data_dblocks_per_idx_read++;

            if (data_dblocks_read%15 == 0){
                memcpy(&cur_index,&fs->dblocks[cur_index*64]+60,4);
                data_dblocks_per_idx_read = 0;
            }
        }
    } else {
        size_t indirect_bytes = upper_bound-256;

        
        size_t total_data_dblocks = ((offset-256) / 64);
        size_t lower_offset_data_dblock_index = total_data_dblocks%15;
        size_t bytes_in_lower_offset_block = (offset-256) % 64;    

        size_t upper_bound_indirect_data_block = (indirect_bytes - 1) / 64; 
        size_t upper_bound_bytes = (indirect_bytes - 1) % 64 + 1;

        size_t data_dblocks_read = total_data_dblocks;
        size_t data_dblocks_per_idx_read = lower_offset_data_dblock_index;

        dblock_index_t cur_index = inode->internal.indirect_dblock;

        for (size_t i = 0; i < calculate_index_dblock_amount(total_data_dblocks); i++){
            memcpy(&cur_index,&fs->dblocks[cur_index*64]+60,4);
        }

        while (data_dblocks_read <= upper_bound_indirect_data_block){
            size_t idx_of_data_dblock = 0;
            memcpy(&idx_of_data_dblock,&fs->dblocks[cur_index*64]+data_dblocks_per_idx_read*4,4);

            if (data_dblocks_read == upper_bound_indirect_data_block){
                memcpy(&fs->dblocks[idx_of_data_dblock*64]+bytes_in_lower_offset_block,buffer+buffer_written,upper_bound_bytes-bytes_in_lower_offset_block);
                buffer_written += upper_bound_bytes-bytes_in_lower_offset_block;
                data_dblocks_read++;
                data_dblocks_per_idx_read++;
                bytes_in_lower_offset_block = 0;
                continue;
            }

            if (data_dblocks_read < (upper_bound_indirect_data_block)){
                memcpy(&fs->dblocks[idx_of_data_dblock*64]+bytes_in_lower_offset_block,buffer+buffer_written,64-bytes_in_lower_offset_block);
                buffer_written += 64-bytes_in_lower_offset_block;
                bytes_in_lower_offset_block = 0;
            }

            data_dblocks_read++;
            data_dblocks_per_idx_read++;

            if (data_dblocks_read%15 == 0){
                memcpy(&cur_index,&fs->dblocks[cur_index*64]+60,4);
                data_dblocks_per_idx_read = 0;
            }
        }
    }

    if ((offset+n)>file_size){
        size_t rem_data = (offset+n)-file_size;
        inode_write_data(fs,inode,buffer,rem_data);
    }
    
    //For the new data, call "inode_write_data" and return
    return SUCCESS;
}

fs_retcode_t inode_shrink_data(filesystem_t *fs, inode_t *inode, size_t new_size)
{

    if (fs == NULL || inode == NULL) return INVALID_INPUT;
    if (new_size>inode->internal.file_size) return INVALID_INPUT;

    //Calculate how many blocks to remove

    if (new_size==0){
        size_t index_to_clear_to = (inode->internal.file_size/64);
        for (size_t i = 0; i <= index_to_clear_to && i < 4; i++){
            release_dblock(fs,&fs->dblocks[inode->internal.direct_data[i]*64]);
        }

        if (index_to_clear_to>=4){
            free_indirect(fs,inode,0);
        }
        inode->internal.file_size = 0;
        return SUCCESS;
    }

    if (new_size<256){
        size_t index_to_clear_to = (inode->internal.file_size/64);
        size_t index_to_clear_from = (new_size/64)+1;
        for (size_t i = index_to_clear_from; i <= index_to_clear_to && i < 4; i++){
            release_dblock(fs,&fs->dblocks[inode->internal.direct_data[i]*64]);
        }

        if (index_to_clear_to>=4){
            free_indirect(fs,inode,0);
        }
    } else {
        size_t new_offset = new_size-256;
        size_t new_offset_index_dblock = calculate_index_dblock_amount(new_offset);
        size_t original_index_dblock = calculate_index_dblock_amount(inode->internal.file_size-256);

        if (new_offset_index_dblock != original_index_dblock){
            free_indirect(fs,inode,new_offset);
        } else {
            size_t size_of_indirect = inode->internal.file_size-256;
            size_t data_dblocks_of_indirect = (size_of_indirect)/64;

            size_t offset_dblocks = ((new_offset+63)/64);            

            dblock_index_t cur_index = inode->internal.indirect_dblock;

            size_t data_dblocks_read = 0;
            for (size_t i = 0; i < calculate_index_dblock_amount(size_of_indirect); i++){
                for (size_t i = 0; i<15 && data_dblocks_read<data_dblocks_of_indirect; i++){
                    if (data_dblocks_read < offset_dblocks) {
                        data_dblocks_read++;
                        continue;
                    }
                    dblock_index_t cur_dblock_idx;
                    memcpy(&cur_dblock_idx,&fs->dblocks[cur_index*64]+i*4,4);
                    release_dblock(fs,&fs->dblocks[cur_dblock_idx*64]);
                    data_dblocks_read++;
                }
                memcpy(&cur_index,&fs->dblocks[cur_index*64]+60,4);
            }
        }
    }

    inode->internal.file_size = new_size;
    return SUCCESS;
}

// make new_size to 0
fs_retcode_t inode_release_data(filesystem_t *fs, inode_t *inode)
{
    if (fs == NULL || inode == NULL) return INVALID_INPUT;
    inode_shrink_data(fs,inode,0);
    return SUCCESS;
}
