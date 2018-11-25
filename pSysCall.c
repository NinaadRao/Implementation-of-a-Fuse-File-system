#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>

#define block_size 512

int inode_bm_offset, data_bm_offset, inode_offset, data_offset;
char temp_block[block_size];

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

typedef struct direntry
{
    int inode_num;
    char filename[28];
} direntry;

void get_superblock()
{
    int fd = open("NNNFS", O_RDONLY);
    if(fd == -1)
    {
        printf("Error opening file");
        return;
    }
    read(fd, temp_block, block_size);
    memcpy(&inode_bm_offset, temp_block, sizeof(int));
    memcpy(&data_bm_offset, temp_block + sizeof(int), sizeof(int));
    memcpy(&inode_offset, temp_block + 2 * sizeof(int), sizeof(int));
    memcpy(&data_offset, temp_block + 3 * sizeof(int), sizeof(int));
    close(fd);
    printf("%d %d %d %d\n", inode_bm_offset, data_bm_offset, inode_offset, data_offset);
}
int get_address_block_inode(int inode_num)
{
    int n = block_size/sizeof(inode);
    n = inode_num/n;
    n = block_size*n;
    n = n + inode_offset;   
    return n;
}
static int search_node(const char *path1)
{
    int current_inode = 0;
    int current_data = 0;
    printf("Entered search_node for path: %s\n", path1);
    if (strcmp(path1, "/") == 0)
        return 0;

    int fd = open("NNNFS", O_RDONLY);
    if(fd == -1)
    {
        printf("Error opening file");
        return -1;
    }
    char* path = (char *)malloc(sizeof(char)*(strlen(path1) + 1));
    strcpy(path, path1);
    path[strlen(path1)] = '\0';
    inode n;
    direntry temp;
    lseek(fd, inode_offset, SEEK_SET);
    read(fd, &n, sizeof(inode));
    char *var = strtok(path, "/");
    //var = strtok(NULL, "/");
    int ino_offset_sum;
    while(var)
    {
        printf("var:%s\n", var);
        int flag = 0;
        if(S_ISREG(n.mode))
        {
            printf("Invalid Path, file does not have children!");
            close(fd);
            free(path);
            return -1;
        }
        int i;
        printf("Data Size: %d", n.size);
        int no = n.size/block_size;
        for(i = 0; i <= no - 1; i++)
        {
            lseek(fd, n.data_block_pointers[i], SEEK_SET);
            for(int j = 0; j < block_size/sizeof(direntry); j++)
            {
                read(fd, &temp, sizeof(direntry));
                printf("direntry: %s\n", temp.filename);
                if(strcmp(var, temp.filename) == 0)
                {
                    flag = 1;
                    ino_offset_sum = temp.inode_num*sizeof(inode) + inode_offset;
                    lseek(fd, ino_offset_sum, SEEK_SET);
                    read(fd, &n, sizeof(inode));
                    break;
                }
            }
            if(flag)
                break;
        }
        if(!flag)
        {
            lseek(fd, n.data_block_pointers[i], SEEK_SET);
            for(int j = 0; j < (n.size%block_size)/sizeof(direntry); j++)
            {
                read(fd, &temp, sizeof(direntry));
                printf("direntry: %s\n", temp.filename);

                if(strcmp(var, temp.filename) == 0)
                {
                    flag = 1;
                    ino_offset_sum = temp.inode_num*sizeof(inode) + inode_offset;
                    lseek(fd, ino_offset_sum, SEEK_SET);
                    read(fd, &n, sizeof(inode));
                    break;
                }
                if(flag)
                    break;
            }
        }
        if(!flag)
        {
            printf("File Not Found!");
            close(fd);
            free(path);
            return -1;
        }
        var = strtok(NULL, "/");
    }
    close(fd);
    free(path);
    return (ino_offset_sum - inode_offset)/sizeof(inode);
}



static int do_getattr(const char *path, struct stat *st )
{
    printf("Entered getattr for path: %s\n\n", path);
    int inode_no = search_node(path);
    if(inode_no == -1)
    {
        printf("getattr couldn't find file\n");
        return -ENOENT;
    }
    int fd = open("NNNFS", O_RDONLY);
    if(fd == -1)
    {
        printf("Error opening file");
        return -1;
    }

    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);
    read(fd, temp_block, block_size);
    close(fd);

    inode n;
    //read(fd, &n, sizeof(inode));
    int x = block_size/sizeof(inode);
    x = inode_no%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));
    st->st_ino=(unsigned int)inode_no;
    st->st_mode=n.mode;
    st->st_nlink=(int) n.hard_link_count;
    st->st_uid=(int) n.uid;
    st->st_gid=(int) n.gid;
    st->st_size=(int) n.size;
    st->st_atime=n.last_access_time;
    st->st_mtime=n.last_modified_time;
    st->st_ctime=n.create_time;
    st->st_blocks=n.block_count;
    return 0;
}

static int do_unlink(const char* path)
{
    int inode_no = search_node(path);
    if(inode_no == -1)
    {
        printf("File does not Exist!\n");
        return -ENOENT;
    }
    int fd = open("NNNFS", O_RDWR);
    inode n;
    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);
    read(fd, temp_block, block_size);
    int x = block_size/sizeof(inode);
    x = inode_no%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));
    char bm_val;
    char *data_bitmap = (char *)malloc(sizeof(char)*block_size);
    for(int i = 0; i < n.block_count; i++)
    {
        int no = n.data_block_pointers[i] - data_offset;
        no = no/block_size;
        
        lseek(fd, data_bm_offset, SEEK_SET);
        read(fd, data_bitmap, block_size);
        
        bm_val = 0;
        memcpy(data_bitmap + no*sizeof(char), &bm_val, sizeof(char));
        lseek(fd, data_bm_offset, SEEK_SET);
        write(fd, data_bitmap, block_size);
    }
    char *inode_bitmap = (char *)malloc(sizeof(char)*block_size);
    lseek(fd, inode_bm_offset, SEEK_SET);
    read(fd, inode_bitmap, block_size);
    memcpy(inode_bitmap + inode_no*sizeof(char), &bm_val, sizeof(char));
    lseek(fd, inode_bm_offset, SEEK_SET);
    write(fd, inode_bitmap, block_size);
    char *buffer = (char *)malloc(sizeof(char)*(strlen(path) + 1));
    strcpy(buffer, path);
    int inode_no_p = search_node(dirname(buffer));
    lseek(fd, get_address_block_inode(inode_no_p), SEEK_SET);
    read(fd, temp_block, block_size);
    x = block_size/sizeof(inode);
    x = inode_no_p%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));

    direntry temp;
    int flag = 0;
    n.size -= sizeof(direntry);
    time_t now;
    time(&now);
    n.last_modified_time = now;
    memcpy(temp_block + x, &n, sizeof(inode));

    lseek(fd, get_address_block_inode(inode_no_p), SEEK_SET);
    write(fd, temp_block, block_size);

     for(int i = 0; i < n.block_count; i++)
    {
        lseek(fd, n.data_block_pointers[i], SEEK_SET);
        read(fd, temp_block, block_size);
        for(int j = 0; j < block_size/sizeof(direntry); j++)
        {
            read(fd, &temp, sizeof(direntry));
            memcpy(&temp, temp_block + j*sizeof(direntry), sizeof(direntry));
            printf("direntry inode: %d\n", temp.inode_num);
            if(inode_no == temp.inode_num)
            {
                flag = 1;
                for(int k = j; k < block_size/sizeof(direntry) - 1; k++)
                {
                    memcpy(&temp, temp_block + (k+1)*sizeof(direntry), sizeof(direntry));
                    memcpy(temp_block + k*sizeof(direntry), &temp, sizeof(direntry));
                }
                lseek(fd, n.data_block_pointers[i], SEEK_SET);
                write(fd, temp_block, block_size);
                break;                
            }
        }
        if(flag)
            break;
    }
    close(fd);
    free(data_bitmap);
    free(inode_bitmap);
    return 0;

}
static int do_rmdir(const char* path)
{
    int inode_no = search_node(path);
    if(inode_no == -1)
    {
        printf("Directory does not Exist!\n");
        return -ENOENT;
    }
    int fd = open("NNNFS", O_RDWR);
    inode n;
    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);
    read(fd, temp_block, block_size);
    int x = block_size/sizeof(inode);
    x = inode_no%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));
    if(n.size != 2*sizeof(direntry))
    {
        printf("Directory Not Empty! Failed to delete!\n");
        close(fd);
        return -ENOTEMPTY;
    }
    int no = n.data_block_pointers[0] - data_offset;
    no = no/block_size;
    char *data_bitmap = (char *)malloc(sizeof(char)*block_size);
    lseek(fd, data_bm_offset, SEEK_SET);
    read(fd, data_bitmap, block_size);
    char bm_val;
    bm_val = 0;
    memcpy(data_bitmap + no*sizeof(char), &bm_val, sizeof(char));
    lseek(fd, data_bm_offset, SEEK_SET);
    write(fd, data_bitmap, block_size);
    char *inode_bitmap = (char *)malloc(sizeof(char)*block_size);
    lseek(fd, inode_bm_offset, SEEK_SET);
    read(fd, inode_bitmap, block_size);
    memcpy(inode_bitmap + inode_no*sizeof(char), &bm_val, sizeof(char));
    lseek(fd, inode_bm_offset, SEEK_SET);
    write(fd, inode_bitmap, block_size);

    char *buffer = (char *)malloc(sizeof(char)*(strlen(path) + 1));
    strcpy(buffer, path);
    int inode_no_p = search_node(dirname(buffer));

    lseek(fd, get_address_block_inode(inode_no_p), SEEK_SET);
    read(fd, temp_block, block_size);
    x = block_size/sizeof(inode);
    x = inode_no_p%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));
    direntry temp;
    int flag = 0;
    n.size -= sizeof(direntry);
    n.hard_link_count -= 1;
    time_t now;
    time(&now);
    n.last_modified_time = now;
    memcpy(temp_block + x, &n, sizeof(inode));

    lseek(fd, get_address_block_inode(inode_no_p), SEEK_SET);
    write(fd, temp_block, block_size);

    for(int i = 0; i < n.block_count; i++)
    {
        lseek(fd, n.data_block_pointers[i], SEEK_SET);
        read(fd, temp_block, block_size);
        for(int j = 0; j < block_size/sizeof(direntry); j++)
        {
            read(fd, &temp, sizeof(direntry));
            memcpy(&temp, temp_block + j*sizeof(direntry), sizeof(direntry));
            printf("direntry inode: %d\n", temp.inode_num);
            if(inode_no == temp.inode_num)
            {
                flag = 1;
                for(int k = j; k < block_size/sizeof(direntry) - 1; k++)
                {
                    memcpy(&temp, temp_block + (k+1)*sizeof(direntry), sizeof(direntry));
                    memcpy(temp_block + k*sizeof(direntry), &temp, sizeof(direntry));
                }
                lseek(fd, n.data_block_pointers[i], SEEK_SET);
                write(fd, temp_block, block_size);
                break;                
            }
        }
        if(flag)
            break;
    }
    close(fd);
    free(data_bitmap);
    free(inode_bitmap);
    return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{   printf("Entered readdir\n\n");
    int inode_no = search_node(path);
    if(inode_no == -1){
        return -ENOENT;
    }
    int fd = open("NNNFS", O_RDONLY);
    if(fd == -1){
        return -1;
    }
    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);
    read(fd, temp_block, block_size);
    inode n;
    direntry temp;
    int x = block_size/sizeof(inode);
    x = inode_no%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));
    int no = n.size/block_size;
    for(int i = 0; i <= no - 1; i++){
        lseek(fd, n.data_block_pointers[i], SEEK_SET);
        read(fd, temp_block, block_size);
        for(int j = 0; j < block_size/sizeof(direntry); j++){        
            memcpy(&temp, temp_block + j*sizeof(direntry), sizeof(direntry));
            filler(buffer, temp.filename, NULL, 0);
        }
    }
    lseek(fd, n.data_block_pointers[no], SEEK_SET);
    read(fd, temp_block, block_size);
    for(int j = 0; j < (n.size%block_size)/sizeof(direntry); j++){
        memcpy(&temp, temp_block + j*sizeof(direntry), sizeof(direntry));
        printf("temp.filename: %s\n", temp.filename);
        
        filler(buffer, temp.filename, NULL, 0);
        printf("Buffer:%s\n",(char *)buffer);    
    }
    close(fd);
    return 0;
}
static int do_mkdir (const char *path, mode_t mode)
{
    fflush(stdout);
    printf("Entered mkdir\n");
    fflush(stdout);
    char *buffer1 = (char *)malloc(sizeof(char)*(strlen(path) + 1));
    strcpy(buffer1, path);
    buffer1[strlen(path)] = '\0';
    if(strcmp(path, buffer1) == 0)
        printf("OMG SAME");

    char *buffer2 = (char *)malloc(sizeof(char)*(strlen(path) + 1));
    strcpy(buffer2, path);
    buffer2[strlen(path)] = '\0';

    int inode_no = search_node(buffer1);
    if(inode_no != -1)
    {
        printf("%d\n",inode_no);
        printf("Cannot Create Directory, same name exists\n");
        free(buffer1);
        free(buffer2);
        return -ENOENT;
    }
    
    inode_no = search_node(dirname(buffer1));
    if(inode_no == -1)
    {
        printf("Directory path does not exist!");
        free(buffer1);
        free(buffer2);
        return -ENOENT;
    }
    int fd = open("NNNFS", O_RDWR);

    char *inode_bitmap = (char *)malloc(block_size*sizeof(char));
    lseek(fd, inode_bm_offset, SEEK_SET);
    read(fd, inode_bitmap, block_size);
    char bm_val;
    int i;
    for(i = 0; i < block_size/sizeof(char); i++)
    {
        memcpy(&bm_val, inode_bitmap + i*sizeof(char), sizeof(char));
        if(!bm_val)
        {
            bm_val = 1;
            memcpy(inode_bitmap + i*sizeof(char), &bm_val, sizeof(char));
            break;
        }

    }
    if(i == block_size/sizeof(char))
    {
        printf("No more space on the disk. Cannot make directory. \n");
        close(fd);
        free(buffer1);
        free(buffer2);
        free(inode_bitmap);
        return -ENOENT;
    }
    lseek(fd, inode_bm_offset, SEEK_SET);
    write(fd, inode_bitmap, block_size);

    inode n;
    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);     //parent directory node block read
    read(fd, temp_block, block_size);                           
    int x = block_size/sizeof(inode);
    x = inode_no%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));                  //parent directory node stored in n
    if(n.size + sizeof(direntry) > 5*block_size)
    {
        printf("Parent directory cannot hold any more directories!\n");
        close(fd);
        free(buffer1);
        free(buffer2);
        free(inode_bitmap);

        return -ENOENT;
    }
    char *data_bitmap = (char *)malloc(sizeof(char)*block_size);
    int j;
    if(n.size/block_size + 1 != n.block_count)
    {
        n.block_count++;
        lseek(fd, data_bm_offset, SEEK_SET);
        read(fd, data_bitmap, block_size);
        for(j = 0; j < block_size/sizeof(char); j++)
        {
            memcpy(&bm_val, data_bitmap + j*sizeof(char), sizeof(char));

            if(!bm_val)
            {
                bm_val = 1;
                memcpy(data_bitmap + j*sizeof(char), &bm_val, sizeof(char));
                break;
            }
        }
        if(j == block_size/sizeof(char))
        {
            printf("NO SPACE, PLEASE GO AWAY");
            close(fd);
            free(buffer1);
            free(buffer2);
            free(inode_bitmap);
            free(data_bitmap);
            return -ENOENT;
        }
        lseek(fd, data_bm_offset, SEEK_SET);
        write(fd, data_bitmap, block_size);
        n.data_block_pointers[n.block_count - 1] = j*sizeof(block_size) + data_offset;            
    }
    n.hard_link_count++;
    int old_size = n.size;
    n.size += sizeof(direntry);

    memcpy(temp_block + x, &n, sizeof(inode));
    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);     
    write(fd,temp_block, block_size);                  //write parent directory block back to file

    
    lseek(fd, n.data_block_pointers[n.block_count - 1], SEEK_SET);
    read(fd, temp_block, block_size);


    direntry temp;
    strcpy(temp.filename, basename(buffer2));
    //temp.filename = basename(buffer2);
    temp.inode_num = i;
    memcpy(temp_block + old_size%block_size, &temp, sizeof(direntry));

    lseek(fd, n.data_block_pointers[n.block_count - 1], SEEK_SET);

    printf("address of parent datablockwrite: %d\noffset: %d\n", n.data_block_pointers[n.block_count - 1] + old_size%block_size, n.data_block_pointers[n.block_count - 1]);
    write(fd, temp_block, block_size);


    lseek(fd, get_address_block_inode(i), SEEK_SET);
    read(fd, temp_block, block_size);
    inode n2;
    time_t now;
    time(&now);

    n2.size = 64;
    n2.last_access_time = now;
    n2.last_modified_time = now;
    n2.create_time = now;
    n2.mode = S_IFDIR|mode;
    n2.hard_link_count = 1;
    n2.block_count = 1;
    n2.uid = getuid();
    n2.gid = getgid();
    //n2.data_block_pointers[0] = 7168;
    n2.data_block_pointers[1] = -1;
    n2.data_block_pointers[2] = -1;
    n2.data_block_pointers[3] = -1;
    n2.data_block_pointers[4] = -1;
    lseek(fd, data_bm_offset, SEEK_SET);
    read(fd, data_bitmap, block_size);
    lseek(fd, data_bm_offset, SEEK_SET);
    read(fd, data_bitmap, block_size);
    for(j = 0; j < block_size/sizeof(char); j++)
    {
        memcpy(&bm_val, data_bitmap + j*sizeof(char), sizeof(char));

        if(!bm_val)
        {
            bm_val = 1;
            memcpy(data_bitmap + j*sizeof(char), &bm_val, sizeof(char));
            break;
        }
    }
    if(j == block_size/sizeof(char))
    {
        printf("NO SPACE, PLEASE GO AWAY");
        close(fd);
        free(buffer1);
        free(buffer2);
        free(inode_bitmap);
        free(data_bitmap);
        return -ENOENT;
    }
    lseek(fd, data_bm_offset, SEEK_SET);
    write(fd, data_bitmap, block_size);
    n2.data_block_pointers[0] = j*block_size + data_offset;
    x = block_size/sizeof(inode);
    x = i%x;
    x = x*sizeof(inode);
    memcpy(temp_block + x,&n2, sizeof(inode)); 
    lseek(fd, get_address_block_inode(i), SEEK_SET);
    write(fd, temp_block, block_size);
    lseek(fd, n2.data_block_pointers[0], SEEK_SET);
    strcpy(temp.filename, ".");
    temp.inode_num = i;
    memcpy(temp_block, &temp, sizeof(direntry));
    strcpy(temp.filename, "..");
    temp.inode_num = inode_no;
    memcpy(temp_block + sizeof(direntry), &temp, sizeof(direntry));
    write(fd, temp_block, block_size);
    close(fd);
    printf("\nINODE NUMBER: %d\nAddress %d\n", i,get_address_block_inode(i) + x);
    free(buffer1);
    free(buffer2);
    free(inode_bitmap);
    free(data_bitmap);
    return 0;
}



     
void free_block(int fd , int block_num, int flag)
{
    if(!flag)
        lseek(fd,inode_bm_offset+block_num,SEEK_SET);
    else 
    {
        block_num=(block_num-data_offset)/block_size;
        lseek(fd,data_bm_offset+block_num,SEEK_SET);
    }
    char y=0;
    write(fd,&y,sizeof(char));

}

int find_free_block(int fd,int flag){
    int count=-1;
    char rd;
    if(!flag)
        lseek(fd,inode_bm_offset,SEEK_SET);
    else
        lseek(fd,data_bm_offset,SEEK_SET);
    do
    {
        read(fd,&rd,sizeof(char));      
        count++;
    }while(rd);
    printf("The count is %d\n",count );
    lseek(fd,data_bm_offset+count,SEEK_SET);
    write(fd,"1",1);
    return count;
}
static int min(int num1,int num2){
    if(num1<num2) return num1;
    else return num2;
}
int create_data_block(int fd,inode *n){
    int data_block_num=find_free_block(fd,1);
    int data_block=(data_offset)+((block_size)*data_block_num);
    int i=0;
    while(n->data_block_pointers[i]!=-1 && i<5)i++;

    if(i==5)return -1;
    n->data_block_pointers[i]=data_block;
    return data_block;
}
static int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{

    printf("----------------------read--------------------------\n\n");
    int inode_no = search_node(path);
    //printf("ReaddirPath:%s\n", path);
    if (inode_no == -1)
    {
        printf("Invalid path\n");
        return -ENOENT;
    }

    int fd = open("NNNFS", O_RDONLY);
    if (fd == -1)
    {
        printf("Error opening file");
        return -1;
    }
    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);
    read(fd, temp_block, block_size);

    inode n;
    // direntry temp;
    int x = block_size / sizeof(inode);
    x = inode_no % x;
    x = x * sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));

    char *file_content = (char *)malloc(sizeof(char) * (size + 1));
    int block_num = 0;
    int i = offset / block_size;
    int no_of_block_to_read = (size - 1) / block_size + 1;
    if (no_of_block_to_read < 0)
    {
        no_of_block_to_read = 0;
    }
    while ((no_of_block_to_read--) && i < 5 && n.data_block_pointers[i] != -1)
    {
        lseek(fd, n.data_block_pointers[i], SEEK_SET);
        read(fd, file_content + block_size * block_num, block_size);
        block_num++;
        i++;
    }

    file_content[size] = '\0';
    printf("Content of file is %s", file_content);
    memcpy(buffer, file_content, size);
    free(file_content);
    close(fd);

    return size;
}

static int do_open(const char *path, struct fuse_file_info *fi)
{
    printf("----------------------------open--------------------------\n");
    int inode_no = search_node(path);
    //printf("ReaddirPath:%s\n", path);
    if (inode_no == -1)
    {
        printf("Invalid path\n");
        return -ENOENT;
    }

    int fd = open("NNNFS", O_RDONLY);
    if (fd == -1)
    {
        printf("Error opening file");
        return -1;
    }
    lseek(fd, get_address_block_inode(inode_no), SEEK_SET);
    read(fd, temp_block, block_size);

    inode n;
    // direntry temp;
    int x = block_size / sizeof(inode);
    x = inode_no % x;
    x = x * sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));

    int temp = n.mode & 4095;
    if (n.uid == getuid())
    {
        if (temp & 256)
        {
            return 0;
        }
        return -EACCES;
    }
    else if (n.gid == getgid())
    {
        if (temp & 32)
        {
            return 0;
        }
        return -EACCES;
    }
    else
    {
        if (temp & 4)
        {
            return 0;
        }
        return -EACCES;
    }
    close(fd);
    return 0;
}

static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi){
    printf("-----------------create------------------\n");
    char *buff=(char *)malloc(sizeof(char)*strlen(path));
    strcpy(buff,path);
    int fd = open("NNNFS",O_RDWR);
    int node_exists=search_node(buff);
    if(node_exists!=-1)return -EEXIST;
    char *buff2=(char *)malloc(sizeof(char)*strlen(path));
    strcpy(buff2,path);
    int parent_inode_num=search_node(dirname(buff2));
    inode parent_inode;
    lseek(fd, get_address_block_inode(parent_inode_num), SEEK_SET);
    read(fd, temp_block, block_size);

    // direntry temp;
    int x = block_size / sizeof(inode);
    x = parent_inode_num % x;
    x = x * sizeof(inode);
    memcpy(&parent_inode, temp_block + x, sizeof(inode));
   // lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
    //read(fd,&parent_inode,sizeof(inode));
    int i=0;
    int start_data_block=parent_inode.size/block_size;
    if(!(parent_inode.size%block_size)){
        printf("Allocate a new data block\n");
    }
    else{
        printf("In else cluause");
        //update entries of parent directory
        char y = 1;
        //find inode number
        int ino_num_child=find_free_block(fd,0);
        printf("Free inode block = %d\n",ino_num_child);
        
        lseek(fd,inode_bm_offset+ino_num_child,SEEK_SET);
        write(fd,&y,sizeof(char));

        //create new entry;
        char *buff3=(char *)malloc(sizeof(char)*strlen(path));
        strcpy(buff3,path);
        direntry d;
        printf("buff %s\n",buff3);
        strcpy(d.filename,basename(buff3));
        printf("Creating new file with name = %s\n",d.filename);
        d.inode_num=ino_num_child;
        int free_dir_ent=(parent_inode.size%block_size)/sizeof(direntry);
        //printf("Write direntry into address: %d\n",parent_inode.data_block_pointers[start_data_block]+(free_dir_ent*sizeof(direntry)));
        lseek(fd,parent_inode.data_block_pointers[start_data_block]+(free_dir_ent*sizeof(direntry)),SEEK_SET);
        int x=write(fd,&d,sizeof(direntry));
        time_t now;
        time(&now);
        /*
        fflush(fd);
        if(fflush(fd)==EOF || !x)
        {
            perror("Did not write\n");
        }
        */
        //fill inode of parent directory
        parent_inode.size=parent_inode.size+sizeof(direntry);
        parent_inode.last_access_time=now;
        parent_inode.last_modified_time=now;


        printf("Write parent inode in address %lu\n",inode_offset+(sizeof(inode)*parent_inode_num));
        printf("New size of parent directory = %d\n",parent_inode.size);
        lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
        x=write(fd,&parent_inode,sizeof(inode));
        /*
        if(fflush(fd)==EOF || !x)
        {
            perror("Did not write\n");
        }
        */
        //for every sub-directory 


        //create a new datablock for directory
        
        int data_block_num=find_free_block(fd,1);
        printf("Free data block = %d\n",data_block_num);
        lseek(fd,data_bm_offset+data_block_num,SEEK_SET);
        write(fd,&y,sizeof(char));
        //fill inode structure

        inode inode_child;
        inode_child.size = 0;
        inode_child.last_access_time = now;
        inode_child.last_modified_time = now;
        inode_child.create_time = now;
        inode_child.mode = S_IFREG|mode;
        inode_child.hard_link_count = 1;
        inode_child.block_count = 1;
        inode_child.uid = getuid();
        inode_child.gid = getgid();
        inode_child.data_block_pointers[0] = data_offset+(block_size*data_block_num);
        inode_child.data_block_pointers[1] = -1;
        inode_child.data_block_pointers[2] = -1;
        inode_child.data_block_pointers[3] = -1;
        inode_child.data_block_pointers[4] = -1;

        lseek(fd, inode_offset+(sizeof(inode)*ino_num_child), SEEK_SET);
        x=write(fd,&inode_child, sizeof(inode));
        /*
        if(fflush(fd)==EOF || !x)
        {
            perror("Did not write\n");
        }
        */
        free(buff3);
    }
    free(buff);free(buff2);//free(buff3);
    close(fd);
    return 0;
}

static int do_link(const char* from, const char* to){
	int from_inode=search_node(from);
	//int from_inode=search_node(from);
	int fd = open("NNNFS", O_RDWR);
	 lseek(fd, get_address_block_inode(from_inode), SEEK_SET);
    read(fd, temp_block, block_size);

    
    inode n;
    //read(fd, &n, sizeof(inode));
    int x = block_size/sizeof(inode);
    x = from_inode%x;
    x = x*sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));
    n.hard_link_count++;
    do_create(to,n.mode,NULL);
	
    memcpy(temp_block+x,&n,sizeof(inode));
     lseek(fd, get_address_block_inode(from_inode), SEEK_SET);
    write(fd, temp_block, block_size);

  close(fd);
  return 0;

}


static int do_write(const char *path, const char *buffer, size_t size, off_t offset,struct fuse_file_info *fileinfo){
    printf("-----------write---------------\n");
    char *loc=malloc(sizeof(char)*strlen(path));
    strcpy(loc,path);
    int ino=search_node(loc);
    if(ino==-1){
        printf("File has to be created\n");
        do_create(path,0755,fileinfo);
    }
    int fd=open("NNNFS",O_RDWR);
    if(fd==-1){
        printf("This is not possible\n");
        return -1;
    }
    inode n;
    lseek(fd, get_address_block_inode(ino), SEEK_SET);
    read(fd, temp_block, block_size);
    int x = block_size / sizeof(inode);
    x = ino % x;
    x = x * sizeof(inode);
    memcpy(&n, temp_block + x, sizeof(inode));
    int flag=0;
    if((n.size+size)>(int)5*block_size){
        printf("File jaasthi big. \n");
        return -EFBIG;
    }
    if(n.size<offset+size){
        flag=1;
    }
    int bytes=0;
    int bytes1=0;
    int offset1 = offset;
    int data=offset/block_size;
    int leftover;
    if(n.size!=0){
        leftover=block_size-(n.size%block_size);
    }
    else{
        leftover=min(block_size,size);
    }
    printf("leftover is %d offset%ld data%d \n",leftover,offset,data );
    if(n.data_block_pointers[data]!=-1){
        int data_block=n.data_block_pointers[data];
        lseek(fd,n.data_block_pointers[data]+n.size%block_size,SEEK_SET);
        int minv=min(leftover,size);
        printf("minv is %d\n",minv );
        bytes+=minv;
        offset+=bytes;

        int minv1 = min(leftover,size);
        bytes1+=minv1;
        offset1+=bytes1;

        printf("offset %ld bytes%d size%ld %d\n",offset,bytes,size,data_block );
        write(fd,buffer,sizeof(char)*minv);
        int count=data+1;
        while(bytes<size){
            printf("Entered while\n");
            data_block=n.data_block_pointers[count];
            if(data_block==-1){
                data_block=create_data_block(fd,&n);//*something that finds the free block*/;//
                if(data_block==-1) return -EFBIG;
                n.block_count++;
                n.data_block_pointers[count]=data_block;
            printf("Entered while %d      %d %d\n",n.block_count,count,data_block);    
            }
            //data_block_pointers
            printf("databl %ld data_block%d  dataoffset %d\n",data_block+(offset/block_size)*block_size,data_block,data_offset );
            lseek(fd,data_block,SEEK_SET);
            printf("String is %s",buffer+bytes);
            printf("offset %ld minv\n",offset );
            int minv=min(block_size,size-bytes);
            write(fd,buffer+bytes,sizeof(char)*minv);
            bytes+=min(block_size,size-bytes);
            // offset+=bytes;
            printf("min=%d block_size %d size %ld  bytes %d\n",min(block_size,size-bytes),block_size,size,bytes );
            bytes1+=min(block_size,size-bytes);
            offset+=min(block_size,minv);
            offset1+=bytes1;

            printf("offset %ld %d\n",offset,offset1 );

            count++;

        }
    }
    else{
        printf("entered here in the else condition\n");
         return -EPERM;
    }
    time_t present;
    time(&present);
    //update file's inode
    printf("%ld\n",offset );
    if(flag)n.size=offset;
    n.last_access_time=present;
    n.last_modified_time=present;
    lseek(fd,inode_offset+(sizeof(inode)*ino),SEEK_SET);
    write(fd,&n,sizeof(inode));
    int parent_inode_num=search_node(dirname(loc));
    inode parent_inode;
    lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
    read(fd,&parent_inode,sizeof(inode));
    parent_inode.last_access_time=present;
    lseek(fd,inode_offset+(sizeof(inode)*parent_inode_num),SEEK_SET);
    write(fd,&parent_inode,sizeof(inode));
    close(fd);
    //printf("BYTES WRITTEN! AND OFFSET AND SIZE%d %d %d\n",bytes,offset,size);
    close(fd);
    return bytes;    
}




static struct fuse_operations operations = {

    .getattr = do_getattr,
    .readdir = do_readdir,
    .mkdir = do_mkdir,
    .rmdir = do_rmdir,
    .create = do_create,
    .write = do_write,
    .open = do_open,
    .read = do_read,
    .unlink = do_unlink,
    .link=do_link
};


int main(int argc, char *argv[])
{
    get_superblock();
    return fuse_main(argc, argv, &operations, NULL);
}