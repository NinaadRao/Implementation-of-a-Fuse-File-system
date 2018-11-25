#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define block_size 512
#define inode_bitmap_offset 512
#define data_bitmap_offset 1024
#define inode_offset 2048
#define data_block_offset 7168

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

}inode;

typedef struct direntry
	{
		int inode_num;
		char filename[28];
	}direntry;


void main()
{
    int fd;

	fd = open("NNNFS", O_CREAT | O_RDWR, 0777 );
	int ibo = inode_bitmap_offset;
	int dbio = data_bitmap_offset;
	int io = inode_offset;
	int dbo = data_block_offset;
	write(fd,&ibo,sizeof(int));
	write(fd,&dbio,sizeof(int));
	write(fd,&io,sizeof(int));
	write(fd,&dbo,sizeof(int));
	char i = 0; 
	lseek(fd, inode_bitmap_offset, SEEK_SET);
	for (int j = 0; j < 2*block_size; ++j)
	{
		write(fd,&i,sizeof(char));// setting the data bitmap and the inode bitmap values initially to 0
	}
	//Creating root
	lseek(fd, inode_bitmap_offset, SEEK_SET);
	i = 1;
	write(fd,&i,sizeof(char));	//updating inode bitmap
	inode inode_root;
	time_t now;
	time(&now);
	inode_root.size = 64;
	inode_root.last_access_time = now;
	inode_root.last_modified_time = now;
	inode_root.create_time = now;
	inode_root.mode = S_IFDIR|0666;
	inode_root.hard_link_count = 2;
	inode_root.block_count = 1;
	inode_root.uid = getuid();
	inode_root.gid = getgid();
	inode_root.data_block_pointers[0] = 7168;
	inode_root.data_block_pointers[1] = -1;
	inode_root.data_block_pointers[2] = -1;
	inode_root.data_block_pointers[3] = -1;
	inode_root.data_block_pointers[4] = -1;
	lseek(fd, inode_offset, SEEK_SET);
	write(fd,&inode_root,sizeof(inode));
	lseek(fd, data_bitmap_offset, SEEK_SET);
	write(fd,&i,sizeof(int));	
	direntry d;
	d.inode_num = 0;
	strcpy(d.filename,".");
	lseek(fd, data_block_offset, SEEK_SET);
	write(fd,&d,sizeof(direntry));
    d.inode_num = 0;
    strcpy(d.filename, "..");
    write(fd, &d, sizeof(direntry));
	close(fd);
	printf("%ld", sizeof(inode));
}