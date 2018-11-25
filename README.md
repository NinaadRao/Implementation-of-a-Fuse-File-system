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

The inode structure used is:<br/>
typedef struct inode<br/>
{<br/>
    mode_t mode;<br/>
    int size;<br/>
    uid_t uid;<br/>
    gid_t gid;<br/>
    time_t last_access_time;<br/>
    time_t last_modified_time;<br/>
    time_t create_time;<br/>
    short int hard_link_count;<br/>
    short int block_count;<br/>
    int data_block_pointers[5];<br/>
<br/>
} inode;<br/>

The dirent structure:<br/>
typedef struct direntry<br/>
{<br/>
    int inode_num;<br/>
    char filename[28];<br/>
} direntry;<br/>

The persistence was implemented by storing the information of the file onto a binary file and this file was read as an nary tree.<br/>
