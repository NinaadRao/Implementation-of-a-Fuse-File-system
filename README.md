# Implementation-of-a-Fuse-File-system
This is a file system implementation using fuse. 
We have implemented persistance.
The system calls implemented include:
1. create
2. read
3. write
4. open
5. opendir
6. readdir
7. mkdir
8. rmdir
9. unlink

The inode structure used is:
typedef struct inode
{
    mode_t mode;
    int size;
    uid_t uid;
    gid_t gid;
    time_t last_access_time;
    time_t last_modified_time;
    time_t create_time;
    short int hard_link_count;
    short int block_count;
    int data_block_pointers[5];

} inode;

The dirent structure:
typedef struct direntry
{
    int inode_num;
    char filename[28];
} direntry;

The persistence was implemented by storing the information of the file onto a binary file and this file was read as an nary tree.
