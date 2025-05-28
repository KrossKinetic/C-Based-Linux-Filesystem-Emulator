# CSE 220: C-Based Linux Filesystem Emulator

This project, developed for CSE 220: Systems Fundamentals I, is an in-memory emulation of a Linux-like filesystem written entirely in C. It involves managing core filesystem structures like i-nodes and data blocks, handling memory allocation, and implementing a hierarchical file and directory system from the ground up.

---

## 🎓 Learning Outcomes

This project was designed to develop a strong understanding of:
* Using helper functions to build a layered application.
* The data structures and concepts behind Linux filesystems.
* Manipulating complex structs and unions in C.
* Advanced C memory management and pointer access.

---

## 💾 Core Filesystem Concepts

The filesystem is built on two fundamental components: **i-nodes** and **d-blocks**.

### I-Nodes (Index Nodes)
I-nodes act as the metadata hub for every file and directory. They don't store the actual content but hold all the essential information about it:
* **File Type**: Whether the i-node represents a regular file (`DATA_FILE`) or a directory (`DIRECTORY`).
* **Permissions**: Read, write, and execute permissions.
* **Name & Size**: The name of the file/directory (up to 14 characters) and its size in bytes.
* **Direct D-Blocks**: An array of pointers to the first few data blocks, allowing for fast access to the beginning of a file's content.
* **Indirect D-Block**: A pointer to a special "index" d-block, which in turn points to more data blocks, allowing files to grow to any size.

### D-Blocks (Data Blocks)
D-blocks are fixed-size (64-byte) chunks of memory where the actual content is stored. There are two kinds:
* **Data D-Blocks**: These blocks store the raw bytes of a file's content or the list of entries for a directory.
* **Index D-Blocks**: These blocks don't store file content. Instead, they store a list of pointers to other data d-blocks, forming a chain that allows a file to expand beyond the limit of its direct blocks.

### File and Directory Representation
* **Files**: A file's i-node points to a series of data blocks that contain the file's raw content.
* **Directories**: A directory's i-node points to data blocks that contain a list of **directory entries**. Each entry is a 16-byte struct containing the name of a child item and a pointer to that child's i-node.
* **Special Entries**: Every directory (except root) contains `.` (a reference to itself) and `..` (a reference to its parent), enabling filesystem traversal.

---

## ✨ Implemented Functionality

The project was broken into several parts, building a complete filesystem API from low-level block manipulation to high-level, shell-like commands.

### Part 1: Low-Level I-Node and Data Block API
This foundational part involved creating an API to manage the data within an i-node's d-blocks. These functions handle all the logic for allocating, finding, and modifying blocks.
* `inode_write_data`: Appends data to an i-node, automatically allocating new direct or indirect d-blocks as needed.
* `inode_read_data`: Reads a specified number of bytes from any offset within an i-node's content.
* `inode_modify_data`: Overwrites or appends data at a specific offset.
* `inode_shrink_data`: Truncates an i-node's content to a new, smaller size, freeing any now-unused d-blocks.
* `inode_release_data`: Frees all d-blocks associated with an i-node.

### Part 2: High-Level File I/O Operations
Building on Part 1, this section implemented a standard file stream interface, similar to `fopen`, `fread`, etc. This introduced the `fs_file_t` object, which tracks an open file and its current read/write offset.
* `fs_open` / `fs_close`: Opens a file by its path and returns a `fs_file_t` handle; closes the handle and frees memory.
* `fs_read` / `fs_write`: Reads or writes data from the file's current offset, updating the offset accordingly.
* `fs_seek`: Moves the file's offset to a position relative to the start, current position, or end of the file.

### Part 3: Filesystem Operations
This part implemented high-level operations that manipulate the filesystem's structure, much like shell commands.
* `new_file` / `new_directory`: Creates a new, empty file or directory at a given path.
* `remove_file` / `remove_directory`: Deletes a file or an empty directory. This involves creating a "tombstone" entry in the parent directory, which can be reused later.
* `change_directory`: Changes the terminal's current working directory.
* `list`: Displays the contents of a directory, showing permissions, size, and name for each entry (similar to `ls -l`).
* `get_path_string`: Returns the absolute path of the current working directory.
* `tree`: Displays a tree-like representation of a directory and all its subdirectories.

---

## 🚀 How to Build and Run

The project is configured to be built with CMake.

1.  **Configure the build (run once):**
    ```bash
    cmake -S . -B build
    ```

2.  **Build the code:**
    ```bash
    cmake --build build
    ```

3.  **Run Tests:**
    Execute the test runners for each part of the assignment:
    ```bash
    ./build/part1_tests
    ./build/part2_tests
    ./build/part3_tests
    ```

---

## CSE220 Completion Score: 87%
