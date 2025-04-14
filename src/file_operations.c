#include "filesys.h"
#include "debug.h"
#include "utility.h"

#include <string.h>

#define DIRECTORY_ENTRY_SIZE (sizeof(inode_index_t) + MAX_FILE_NAME_LEN)
#define DIRECTORY_ENTRIES_PER_DATABLOCK (DATA_BLOCK_SIZE / DIRECTORY_ENTRY_SIZE)

// ----------------------- HELPER FUNCTION --------------------- //
void print_string_from_void(void* ptr) {
    if (ptr == NULL) {
        fprintf(stderr,"Error: NULL pointer provided\n");
        return;
    }
    
    // Cast the void pointer to a char pointer
    char* str_ptr = (char*)ptr;
    
    // Print the content as a string
    fprintf(stderr,"\n%s\n", str_ptr);
}

void format_token_for_comparison(char* token, char formatted_name[14]) {
    if (token == NULL) {
        memset(formatted_name, 0, 14);
        return;
    }
    size_t len = strlen(token);
    if (len >= 14) {
        memcpy(formatted_name, token, 14);
    } else {
        strcpy(formatted_name, token);
        memset(formatted_name + len + 1, ' ', 14 - len - 1);
    }
}

char* format_filename(char* filename) {
    if (filename == NULL) {
        return NULL;
    }    
    size_t len = strlen(filename);
    if (len < 14) {
        memset(filename + len + 1, ' ', 14 - len - 1);
    }
    return filename;
}

int count_char_occurrences(const char *str, char target) {
    int count = 0;
    if (str != NULL) {
        for (int i = 0; str[i] != '\0'; i++) {
            if (str[i] == target) {
                count++;
            }
        }
    }
    return count;
}

inode_t* return_inode(char *path, inode_t* dir, filesystem_t* fs){
    int slashes = count_char_occurrences(path,'/');
    char * token = strtok(path, "/");
    char formatted_name[14];
    format_token_for_comparison(token,formatted_name);
    inode_t *cur_inode = dir;
    int is_file = 0;
    size_t bytes_read = 0;
    while(token != NULL && slashes>=0) {
        
        if (slashes == 0){
            is_file = 1;
        }
        
        byte *all_data = malloc(cur_inode->internal.file_size);
        inode_read_data(fs,cur_inode,0,all_data,cur_inode->internal.file_size,&bytes_read);
        byte *original_ptr = all_data;
        for (size_t i = 0; i < (bytes_read)/16; i++){
            inode_index_t inode_inside_idx;
            memcpy(&inode_inside_idx,all_data,2);
            all_data += 2;

            char name_of_inode[14];
            memcpy(name_of_inode, all_data, 14);
            format_filename(name_of_inode);
            all_data+=14;

            if (is_file == 0){
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    cur_inode = next_inode;
                    break;
                }
            } else {
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    return next_inode;
                }
            }

        }
        format_token_for_comparison(strtok(NULL, "/"),formatted_name);
        free(original_ptr);
        slashes--;
    }
    return NULL;
}

int verify_path(char *path, inode_t* dir, filesystem_t* fs){
    int slashes = count_char_occurrences(path,'/');
    
    char * token = strtok(path, "/");
    char formatted_name[14];
    format_token_for_comparison(token,formatted_name);
    
    inode_t *cur_inode = dir;
    
    int is_file = 0;
    int file_found = 0;

    size_t bytes_read = 0;

    //int count = 0;
    while(token != NULL && slashes>=0) {

        if (slashes == 0){
            is_file = 1;
        }

        byte *all_data = malloc(cur_inode->internal.file_size);
        inode_read_data(fs,cur_inode,0,all_data,cur_inode->internal.file_size,&bytes_read);
        byte *original_ptr = all_data;

        for (size_t i = 0; i < (bytes_read)/16; i++){
            inode_index_t inode_inside_idx;
            memcpy(&inode_inside_idx,all_data,2);
            all_data += 2;

            char name_of_inode[14];
            memcpy(name_of_inode, all_data, 14);
            format_filename(name_of_inode);
            all_data+=14;

            if (is_file == 0){
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    if (next_inode->internal.file_type == DIRECTORY){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else {
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    
                    if (next_inode->internal.file_type != DATA_FILE){
                        REPORT_RETCODE(INVALID_FILE_TYPE);
                        return 0;
                    }

                    if (next_inode->internal.file_type == DATA_FILE){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            }

        }

        if (file_found == 0 && is_file == 0){
            REPORT_RETCODE(DIR_NOT_FOUND);
            return 0;
        } else if (file_found == 0 && is_file == 1){
            REPORT_RETCODE(FILE_NOT_FOUND);
            return 0;
        }

        format_token_for_comparison(strtok(NULL, "/"),formatted_name);

        free(original_ptr);
        slashes--;
        file_found = 0;
    }
    return 1;
}

inode_t* return_inode_v2(char *path, inode_t* dir, filesystem_t* fs){
    int slashes = count_char_occurrences(path,'/');
    char * token = strtok(path, "/");
    char formatted_name[14];
    format_token_for_comparison(token,formatted_name);
    inode_t *cur_inode = dir;
    int is_file = 0;
    size_t bytes_read = 0;
    while(token != NULL && slashes>=0) {
        
        if (slashes == 1){
            is_file = 1;
        }
        
        byte *all_data = malloc(cur_inode->internal.file_size);
        inode_read_data(fs,cur_inode,0,all_data,cur_inode->internal.file_size,&bytes_read);
        byte *original_ptr = all_data;
        for (size_t i = 0; i < (bytes_read)/16; i++){
            inode_index_t inode_inside_idx;
            memcpy(&inode_inside_idx,all_data,2);
            all_data += 2;

            char name_of_inode[14];
            memcpy(name_of_inode, all_data, 14);
            format_filename(name_of_inode);
            all_data+=14;

            if (is_file == 0){
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    cur_inode = next_inode;
                    break;
                }
            } else {
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    return next_inode;
                }
            }

        }
        format_token_for_comparison(strtok(NULL, "/"),formatted_name);
        free(original_ptr);
        slashes--;
    }
    return NULL;
}

int verify_path_v2(char *path, inode_t* dir, filesystem_t* fs){
    int slashes = count_char_occurrences(path,'/');
    
    char * token = strtok(path, "/");
    char formatted_name[14];
    format_token_for_comparison(token,formatted_name);
    
    inode_t *cur_inode = dir;
    
    int is_file = 0;
    int file_found = 0;

    size_t bytes_read = 0;

    //int count = 0;
    while(token != NULL && slashes>=0) {

        if (slashes == 0){
            is_file = 1;
        }

        byte *all_data = malloc(cur_inode->internal.file_size);
        inode_read_data(fs,cur_inode,0,all_data,cur_inode->internal.file_size,&bytes_read);
        byte *original_ptr = all_data;

        for (size_t i = 0; i < (bytes_read)/16; i++){
            inode_index_t inode_inside_idx;
            memcpy(&inode_inside_idx,all_data,2);
            all_data += 2;

            char name_of_inode[14];
            memcpy(name_of_inode, all_data, 14);
            format_filename(name_of_inode);
            all_data+=14;

            if (is_file == 0){
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    if (next_inode->internal.file_type == DIRECTORY){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else {
                if (strcmp(formatted_name,name_of_inode) == 0){
                   REPORT_RETCODE(FILE_EXIST);
                   return 0;
                }
            }

        }

        if (file_found == 0 && is_file == 0){
            REPORT_RETCODE(DIR_NOT_FOUND);
            return 0;
        } 

        format_token_for_comparison(strtok(NULL, "/"),formatted_name);

        free(original_ptr);
        slashes--;
        file_found = 0;
    }
    return 1;
}

int verify_path_v3(char *path, inode_t* dir, filesystem_t* fs){
    int slashes = count_char_occurrences(path,'/');
    
    char * token = strtok(path, "/");
    char formatted_name[14];
    format_token_for_comparison(token,formatted_name);
    
    inode_t *cur_inode = dir;
    
    int is_file = 0;
    int file_found = 0;

    size_t bytes_read = 0;

    //int count = 0;
    while(token != NULL && slashes>=0) {

        if (slashes == 0){
            is_file = 1;
        }

        byte *all_data = malloc(cur_inode->internal.file_size);
        inode_read_data(fs,cur_inode,0,all_data,cur_inode->internal.file_size,&bytes_read);
        byte *original_ptr = all_data;

        for (size_t i = 0; i < (bytes_read)/16; i++){
            inode_index_t inode_inside_idx;
            memcpy(&inode_inside_idx,all_data,2);
            all_data += 2;

            char name_of_inode[14];
            memcpy(name_of_inode, all_data, 14);
            format_filename(name_of_inode);
            all_data+=14;

            if (is_file == 0){
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    if (next_inode->internal.file_type == DIRECTORY){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else {
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    
                    if (next_inode->internal.file_type != DATA_FILE){
                        REPORT_RETCODE(INVALID_FILE_TYPE);
                        return 0;
                    }

                    if (next_inode->internal.file_type == DATA_FILE){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            }

        }

        if (file_found == 0 && is_file == 0){
            REPORT_RETCODE(DIR_NOT_FOUND);
            return 0;
        } else if (file_found == 0 && is_file == 1){
            REPORT_RETCODE(NOT_FOUND);
            return 0;
        }

        format_token_for_comparison(strtok(NULL, "/"),formatted_name);

        free(original_ptr);
        slashes--;
        file_found = 0;
    }
    return 1;
}

int verify_path_v4(char *path, inode_t* dir, filesystem_t* fs){
    int slashes = count_char_occurrences(path,'/');
    
    char * token = strtok(path, "/");
    char formatted_name[14];
    format_token_for_comparison(token,formatted_name);
    
    inode_t *cur_inode = dir;
    
    int is_file = 0;
    int file_found = 0;

    size_t bytes_read = 0;

    //int count = 0;
    while(token != NULL && slashes>=0) {

        if (slashes == 0){
            is_file = 1;
        }

        byte *all_data = malloc(cur_inode->internal.file_size);
        inode_read_data(fs,cur_inode,0,all_data,cur_inode->internal.file_size,&bytes_read);
        byte *original_ptr = all_data;

        for (size_t i = 0; i < (bytes_read)/16; i++){
            inode_index_t inode_inside_idx;
            memcpy(&inode_inside_idx,all_data,2);
            all_data += 2;

            char name_of_inode[14];
            memcpy(name_of_inode, all_data, 14);
            format_filename(name_of_inode);
            all_data+=14;

            if (is_file == 0){
                if (strcmp(formatted_name,name_of_inode) == 0){
                    
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];

                    if (next_inode->internal.file_type != DIRECTORY){
                        REPORT_RETCODE(DIR_NOT_FOUND);
                        return 0;
                    }

                    if (next_inode->internal.file_type == DIRECTORY){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else {
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    
                    if (next_inode->internal.file_type != DIRECTORY){
                        REPORT_RETCODE(DIR_NOT_FOUND);
                        return 0;
                    }

                    if (cur_inode->internal.file_size > 32){
                        REPORT_RETCODE(DIR_NOT_EMPTY);
                        return 1;
                    } 

                    else return 0;

                    if (next_inode->internal.file_type == DIRECTORY){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            }

        }

        if (file_found == 0 && is_file == 0){
            REPORT_RETCODE(DIR_NOT_FOUND);
            return 0;
        } else if (file_found == 0 && is_file == 1){
            REPORT_RETCODE(DIR_NOT_FOUND);
            return 0;
        }

        format_token_for_comparison(strtok(NULL, "/"),formatted_name);

        free(original_ptr);
        slashes--;
        file_found = 0;
    }
    return 1;
}

int verify_path_v2_1(char *path, inode_t* dir, filesystem_t* fs){
    int slashes = count_char_occurrences(path,'/');
    
    char * token = strtok(path, "/");
    char formatted_name[14];
    format_token_for_comparison(token,formatted_name);
    
    inode_t *cur_inode = dir;
    
    int is_file = 0;
    int file_found = 0;

    size_t bytes_read = 0;

    //int count = 0;
    while(token != NULL && slashes>=0) {

        if (slashes == 0){
            is_file = 1;
        }

        byte *all_data = malloc(cur_inode->internal.file_size);
        inode_read_data(fs,cur_inode,0,all_data,cur_inode->internal.file_size,&bytes_read);
        byte *original_ptr = all_data;

        for (size_t i = 0; i < (bytes_read)/16; i++){
            inode_index_t inode_inside_idx;
            memcpy(&inode_inside_idx,all_data,2);
            all_data += 2;

            char name_of_inode[14];
            memcpy(name_of_inode, all_data, 14);
            format_filename(name_of_inode);
            all_data+=14;

            if (is_file == 0){
                if (strcmp(formatted_name,name_of_inode) == 0){
                    inode_t *next_inode = &fs->inodes[inode_inside_idx];
                    if (next_inode->internal.file_type == DIRECTORY){
                        cur_inode = next_inode;
                        file_found = 1;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else {
                if (strcmp(formatted_name,name_of_inode) == 0){
                   REPORT_RETCODE(DIRECTORY_EXIST);
                   return 0;
                }
            }

        }

        if (file_found == 0 && is_file == 0){
            REPORT_RETCODE(DIR_NOT_FOUND);
            return 0;
        } 

        format_token_for_comparison(strtok(NULL, "/"),formatted_name);

        free(original_ptr);
        slashes--;
        file_found = 0;
    }
    return 1;
}

// ----------------------- CORE FUNCTION ----------------------- //
int new_file(terminal_context_t *context, char *path, permission_t perms)
{

    if (context == NULL || path == NULL){
        return 0;
    } 

    if (verify_path_v2(path,context->working_directory,context->fs) == 0){
        return -1;
    }

    inode_index_t new_inode_idx;

    fprintf(stderr,"path= %s",path);
    
    if (claim_available_inode(context->fs, &new_inode_idx) != SUCCESS) {
        REPORT_RETCODE(INODE_UNAVAILABLE);
        return -1;
    }

    /*
    inode_t* new_inode = &context->fs->inodes[new_inode_idx];
    inode_t *cur_inode;

    if (count_char_occurrences(path,'/') == 0){
        cur_inode = context->working_directory;
    } else {
        cur_inode = return_inode_v2(path,context->working_directory,context->fs);
    }

    new_inode->internal.file_type = DATA_FILE;
    new_inode->internal.file_perms = perms;

    char* basename = strrchr(path, '/');
    
    if (basename == NULL) {
        basename = path;
    } else {
        basename++;
    }
        */
    
   return 0;
}

int new_directory(terminal_context_t *context, char *path)
{
    if (context == NULL || path == NULL){
        return 0;
    } 

    if (verify_path_v2_1(path,context->working_directory,context->fs) == 0){
        return -1;
    }

    inode_index_t new_inode_idx;

    fprintf(stderr,"path= %s",path);
    
    if (claim_available_inode(context->fs, &new_inode_idx) != SUCCESS) {
        REPORT_RETCODE(INODE_UNAVAILABLE);
        return -1;
    }

    return -2;
}

int remove_file(terminal_context_t *context, char *path)
{
    if (context == NULL || path == NULL){
        return 0;
    } 

    if (verify_path(path,context->working_directory,context->fs) == 0){
        return -1;
    }

    return -1;
}

// we can only delete a directory if it is empty!!
int remove_directory(terminal_context_t *context, char *path)
{
    if (context == NULL || path == NULL){
        return 0;
    } 

    if (verify_path_v4(path,context->working_directory,context->fs) == 0){
        return -1;
    }

    if (return_inode(path,context->working_directory,context->fs) == context->working_directory){
        REPORT_RETCODE(ATTEMPT_DELETE_CWD);
        return -1;
    }

    //inode_t *cur_inode = context->working_directory;

    return -1;
}

int change_directory(terminal_context_t *context, char *path)
{
    if (context == NULL || path == NULL){
        return 0;
    } 

    if (verify_path_v4(path,context->working_directory,context->fs) == 0){
        return -1;
    }

    return -2;
}

int list(terminal_context_t *context, char *path)
{
    if (context == NULL || path == NULL){
        return 0;
    } 

    if (verify_path_v3(path,context->working_directory,context->fs) == 0){
        return -1;
    }

    return -2;
}

char *get_path_string(terminal_context_t *context) {
    if (context == NULL) {
        char *empty_string = malloc(1);
        if (empty_string) {
            empty_string[0] = '\0';
        }
        return empty_string;
    }

    if (context->working_directory == &context->fs->inodes[0]) {
        char *root_path = malloc(5);
        if (root_path) {
            strcpy(root_path, "root");
        }
        return root_path;
    }

    typedef struct {
        inode_index_t inode_idx;
        char name[14];
    } dir_entry_t;

    typedef struct name_node {
        char name[15];
        size_t name_len;
        struct name_node *next;
    } name_node_t;

    name_node_t *stack = NULL;

    inode_t *current_dir = context->working_directory;

    while (current_dir != &context->fs->inodes[0]) {
        dir_entry_t parent_entry;
        size_t bytes_read;
    
        if (inode_read_data(context->fs, current_dir, 16, &parent_entry, sizeof(dir_entry_t), &bytes_read) != SUCCESS) {
            while (stack) {
                name_node_t *temp = stack;
                stack = stack->next;
                free(temp);
            }
            char *empty = malloc(1);
            if (empty) empty[0] = '\0';
            return empty;
        }
        
        inode_t *parent_dir = &context->fs->inodes[parent_entry.inode_idx];
        

        dir_entry_t entry;
        size_t offset = 0;
        int found = 0;
        
        while (offset < parent_dir->internal.file_size) {
            if (inode_read_data(context->fs, parent_dir, offset, 
                              &entry, sizeof(dir_entry_t), &bytes_read) != SUCCESS) {
                break;
            }
            
            if (entry.inode_idx == (current_dir - context->fs->inodes)) {
                if (!(entry.name[0] == '.' && entry.name[1] == '\0') && 
                    !(entry.name[0] == '.' && entry.name[1] == '.' && entry.name[2] == '\0')) {

                    name_node_t *new_node = malloc(sizeof(name_node_t));
                    if (!new_node) {
                        while (stack) {
                            name_node_t *temp = stack;
                            stack = stack->next;
                            free(temp);
                        }
                        char *empty = malloc(1);
                        if (empty) empty[0] = '\0';
                        return empty;
                    }
                    
                    memcpy(new_node->name, entry.name, 14);
                    
                    size_t name_len = 14;
                    for (size_t i = 0; i < 14; i++) {
                        if (entry.name[i] == '\0') {
                            name_len = i;
                            break;
                        }
                    }
                    
                    new_node->name_len = name_len;
                    new_node->name[14] = '\0';
                    
                    new_node->next = stack;
                    stack = new_node;
                    
                    found = 1;
                    break;
                }
            }
            
            offset += sizeof(dir_entry_t);
        }
        
        if (!found) {
            while (stack) {
                name_node_t *temp = stack;
                stack = stack->next;
                free(temp);
            }
            char *empty = malloc(1);
            if (empty) empty[0] = '\0';
            return empty;
        }
        
        current_dir = parent_dir;
    }

    size_t path_len = 4;
    name_node_t *node = stack;
    while (node) {
        path_len += node->name_len + 1;
        node = node->next;
    }
    
    char *path = malloc(path_len + 1);
    if (!path) {
        while (stack) {
            name_node_t *temp = stack;
            stack = stack->next;
            free(temp);
        }
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    strcpy(path, "root");
    size_t current_pos = 4;
    
    while (stack) {
        path[current_pos] = '/';
        current_pos++;
        
        name_node_t *temp = stack;
        stack = stack->next;
        
        memcpy(path + current_pos, temp->name, temp->name_len);
        current_pos += temp->name_len;
        
        free(temp);
    }
    path[current_pos] = '\0';
    return path;
}

int tree(terminal_context_t *context, char *path)
{
    if (context == NULL || path == NULL){
        return 0;
    } 

    if (verify_path_v3(path,context->working_directory,context->fs) == 0){
        return -1;
    }

    printf("root");

    return -1;
}

//Part 2
void new_terminal(filesystem_t *fs, terminal_context_t *term)
{
    if (fs==NULL) return;
    term->fs = fs;
    term->working_directory = &(fs->inodes[0]);
}

fs_file_t fs_open(terminal_context_t *context, char *path)
{
    char* path_copy = NULL;
    if (path != NULL) path_copy = strdup(path);

    if (context==NULL){
        return NULL;
    }
    
    if (path==NULL){
        REPORT_RETCODE(DIR_NOT_FOUND);
        return NULL;
    }

    if (verify_path(path,context->working_directory,context->fs) == 0){
        return NULL;
    }

    fs_file_t file = (struct fs_file*)malloc(sizeof(struct fs_file));

    file->offset = 0;
    file->inode = return_inode(path_copy, context->working_directory, context->fs);
    file->fs = context->fs;

    return file;
}

void fs_close(fs_file_t file)
{
    if (file == NULL) return;
    free(file);
}

size_t fs_read(fs_file_t file, void *buffer, size_t n)
{
    if (file == NULL) return 0;
    (void)file;
    (void)buffer;
    (void)n;

    size_t offset = file->offset;
    size_t file_size = file->inode->internal.file_size;
    size_t bytes_read = 0;
    inode_read_data(file->fs, file->inode, file->offset, buffer, n, &bytes_read);

    if ((offset+n) > file_size){
        bytes_read = file_size-offset;
    }

    file->offset += bytes_read;
    
    return bytes_read;
}

size_t fs_write(fs_file_t file, void *buffer, size_t n)
{
    if (file == NULL) return 0;

    fs_retcode_t ret = inode_modify_data(file->fs, file->inode, file->offset, buffer, n);
    
    if (ret != SUCCESS) {
        return 0;  // Return 0 bytes written
    }
    
    file->offset += n;
    
    return n;  // Return the number of bytes written
}

int fs_seek(fs_file_t file, seek_mode_t seek_mode, int offset)
{
    if (file == NULL) return -1;
    if (seek_mode>4) return -1;

    long new_offset = 0; 
    size_t file_size = file->inode->internal.file_size;

    // Calculate new position based on seek mode
    if (seek_mode == FS_SEEK_START) {
        new_offset = offset;
    } else if (seek_mode == FS_SEEK_CURRENT) {
        new_offset = (long)file->offset + offset;
    } else if (seek_mode == FS_SEEK_END) {
        if (offset<=0){
            offset = offset*-1;
            new_offset = (long)file_size - offset;
        } else {
            new_offset = file_size;
        }
    } else {
        return -1;
    }

    if (new_offset < 0) {
        return -1;
    }

    if ((size_t)new_offset > file_size) {
        file->offset = file_size;
    } else {
        file->offset = (size_t)new_offset;
    }

    return 0; // Success
}

