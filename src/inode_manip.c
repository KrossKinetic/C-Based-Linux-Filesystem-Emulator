#include "filesys.h"

#include <string.h>
#include <assert.h>

#include "utility.h"
#include "debug.h"

#define INDIRECT_DBLOCK_INDEX_COUNT (DATA_BLOCK_SIZE / sizeof(dblock_index_t) - 1)
#define INDIRECT_DBLOCK_MAX_DATA_SIZE ( DATA_BLOCK_SIZE * INDIRECT_DBLOCK_INDEX_COUNT )

#define NEXT_INDIRECT_INDEX_OFFSET (DATA_BLOCK_SIZE - sizeof(dblock_index_t))

// ----------------------- UTILITY FUNCTION ----------------------- //



// ----------------------- CORE FUNCTION ----------------------- //

void write_to_dblock(filesystem_t *fs, dblock_index_t dblock_idx, size_t offset_val, void *data, size_t n){
    memcpy(&fs->dblocks[(dblock_idx*64)+offset_val],data,n);
}

fs_retcode_t inode_write_data(filesystem_t *fs, inode_t *inode, void *data, size_t n)
{
    (void)fs; // FileSystem
    (void)inode; // This is where you write to
    (void)n; // Number of bytes to write
    (void)data; // Data to write from

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

            // Write Remaining Data to Indirect Dblock
            size_t dblocks_needed = calculate_necessary_dblock_amount(n); // (idx dblock + data dblock)
            size_t idx_dblocks_needed = calculate_index_dblock_amount(n); // (idx dblock)
            size_t data_dblocks_needed = (dblocks_needed-idx_dblocks_needed);

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
                    data_dblocks_needed--;
                }

                temp_previous_idx_dblock = temp_idx_dblock;
            }
        }
    }

    return SUCCESS;
}

fs_retcode_t inode_read_data(filesystem_t *fs, inode_t *inode, size_t offset, void *buffer, size_t n, size_t *bytes_read)
{
    (void)fs;
    (void)inode;
    (void)offset;
    (void)buffer;
    (void)n;
    (void)bytes_read;
    return NOT_IMPLEMENTED;
    
    //check to make sure inputs are valid

    //for 0 to n, use the helper function to read and copy 1 byte at a time
}

fs_retcode_t inode_modify_data(filesystem_t *fs, inode_t *inode, size_t offset, void *buffer, size_t n)
{
    (void)fs;
    (void)inode;
    (void)offset;
    (void)buffer;
    (void)n;
    return NOT_IMPLEMENTED;

    //check to see if the input is valid

    //calculate the final filesize and verify there are enough blocks to support it
    //use calculate_necessary_dblock_amount and available_dblocks


    //Write to existing data in your inode

    //For the new data, call "inode_write_data" and return
}

fs_retcode_t inode_shrink_data(filesystem_t *fs, inode_t *inode, size_t new_size)
{
    (void)fs;
    (void)inode;
    (void)new_size;
    return NOT_IMPLEMENTED;
    
    //check to see if inputs are in valid range

    //Calculate how many blocks to remove

    //helper function to free all indirect blocks

    //remove the remaining direct dblocks

    //update filesize and return
}

// make new_size to 0
fs_retcode_t inode_release_data(filesystem_t *fs, inode_t *inode)
{
    (void)fs;
    (void)inode;
    return NOT_IMPLEMENTED;
    //shrink to size 0
}
