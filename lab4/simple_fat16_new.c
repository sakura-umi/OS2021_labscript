//#define _FILE_OFFSET_BITS 64
#define de(_NAME_, _VALUE_) printf("--%s: Step %lf--\n", _NAME_, _VALUE_);
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "fat16.h"
#include "bench.h"
char *FAT_FILE_NAME = "fat16.img";

/* 将扇区号为secnum的扇区读到buffer中 */

void sector_read(FILE *fd, unsigned int secnum, void *buffer)
{
    fseek(fd, BYTES_PER_SECTOR * secnum, SEEK_SET);
    fread(buffer, BYTES_PER_SECTOR, 1, fd);
}

/** TODO:
 * 将输入路径按“/”分割成多个字符串，并按照FAT文件名格式转换字符串
 * 输入: pathInput: char*, 输入的文件路径名, 如/home/user/m.c
 * 输出: pathDepth_ret, 文件层次深度, 如 3
 * 返回: 按FAT格式转换后的文件名字符串.
 *
 * Hint1:假设pathInput为“/dir1/dir2/file.txt”，则将其分割成“dir1”，“dir2”，“file.txt”，
 *      每个字符串转换成长度为11的FAT格式的文件名，如“file.txt”转换成“FILE    TXT”，
 *      返回转换后的字符串数组，并将*pathDepth_ret设置为3, 转换格式在文档中有说明.
 * Hint2:可能会出现过长的字符串输入，如“/.Trash-1000”，需要自行截断字符串
 * Hint3:需要考虑.和..的情况(. 和 .. 的输入应当为 /.和/..)
 **/
int split_string(char* string, char *sep, char** string_clips) {

    string_clips[0] = strtok(string, sep);
    int clip_num=0;

    do {
        char *head, *tail;
        head = string_clips[clip_num];
        tail = head + strlen(string_clips[clip_num]) - 1;
        while(*head == ' ' && head != tail)
            head ++;
        while(*tail == ' ' && tail != head)
            tail --;
        *(tail + 1) = '\0';
        string_clips[clip_num] = head;
        clip_num ++;
    } while (string_clips[clip_num] = strtok(NULL, sep));

    return clip_num;
}

int depth(const char *pathInput)
{
	int pathDepth = 0;
	int i = 0;
	while(pathInput[i] != '\0')
	{
		if(pathInput[i++] == '/')
	    pathDepth++;
  }
	return pathDepth;
}

char **path_split(char *pathInput, int *pathDepth_ret)
{
    int i = 0, j = 0, k = 0;
    int pathDepth = 0;

    pathDepth = depth(pathInput);

    char **paths = malloc(pathDepth * sizeof(char *));

    char split_flag[2] = "/";
    char *temp;
    temp = strtok(pathInput,split_flag);
    for(i = 0;i < pathDepth;i++)
		{
        paths[i] = temp;
        temp = strtok(NULL,split_flag);
    }

    char ** path_transform = (char ** ) malloc(pathDepth * sizeof(char *));
    for (i = 0; i < pathDepth; i++)
        path_transform[i] = (char *) malloc(11 * sizeof(char));

		//cgar changed to FAT16 form

    int flag = 0; // judge if path is .
    for (i = 0; i < pathDepth; i++)
		{
				int name_len = 0;   //文件名长度
        int ext_len = 0;     //拓展名长度

        for (j = 0, k = 0; ; j++, k++)
				{
            if (paths[i][j] == '.')
						{
                if (j == 0 && paths[i][j + 1] == '\0') //it is a . file
								{
                    path_transform[i][0] = '.';
                		for (k = 1; k < 11; k++)
                    		path_transform[i][k] = ' ';
										//first is . and all space after .
               			break;
                }

                //judge if it is .. file
                if (j == 0 && paths[i][j + 1] == '.' && paths[i][j + 2] == '\0')
								{
                    path_transform[i][0] = '.';
                    path_transform[i][1] = '.';
                    for (k = 2; k < 11; k++)
                        path_transform[i][k] = ' ';
										//first two is .. and all space after ..
										//We should not change the form under these two situations above
                    break;
                }

                if (!flag)
								{
                    if (paths[i][j + 1] == '\0')
										{
                        printf("%s:This file has no extension name\n", paths[i]);
                        exit(1);
												//example: a file named "abc."
                    }
										flag = 1;
										// change flag to 1

                    for (; k < 8; k++)
                        path_transform[i][k] = ' ';
                    k = 7;
                }
                else {
                    printf("%s: More than one dot character (.) \n", paths[i]);
                    exit(1);
                }
            }

            else if (paths[i][j] == '\0') {
                for (; k < 11; k++)
                    path_transform[i][k] = ' ';
                break;
            }
            else if (paths[i][j] >= 'a' && paths[i][j] <= 'z')
						{
                if(name_len == 8 && !flag)
								{
										k = 7;
                    continue;
								}
								if(ext_len == 3)
                    break;
                path_transform[i][k] = paths[i][j] - 32;
                if (flag)
                    ext_len++;
                else
                    name_len++;
						}
            else
						{
								if(name_len==8 && !flag)
                {
										k = 7;
										continue;
								}
                if(ext_len==3)
                    break;
                path_transform[i][k] = paths[i][j];
                if (flag)
                    ext_len++;
                else
                    name_len++;
            }
    		}/*
				if(name_len == 8 && ext_len == 0)
				{
					path_transform[i][8] = ' ';
					path_transform[i][9] = ' ';
					path_transform[i][10] = ' ';
				}*/
  	}

    *pathDepth_ret = pathDepth;
    free(paths);
    return path_transform;
}
/**
 * 将FAT文件名格式解码成原始的文件名
 *
 * 假设path是“FILE    TXT”，则返回"file.txt"
 **/
BYTE *path_decode(BYTE *path)
{
    int i, j;
    BYTE *pathDecoded = malloc(MAX_SHORT_NAME_LEN * sizeof(BYTE));

    /* If the name consists of "." or "..", return them as the decoded path */
    if (path[0] == '.' && path[1] == '.' && path[2] == ' ')
    {
        pathDecoded[0] = '.';
        pathDecoded[1] = '.';
        pathDecoded[2] = '\0';
        return pathDecoded;
    }
    if (path[0] == '.' && path[1] == ' ')
    {
        pathDecoded[0] = '.';
        pathDecoded[1] = '\0';
        return pathDecoded;
    }

    /* Decoding from uppercase letters to lowercase letters, removing spaces,
     * inserting 'dots' in between them and verifying if they are legal */
    for (i = 0, j = 0; i < 11; i++)
    {
        if (path[i] != ' ')
        {
            if (i == 8 && path[8] != ' ')
                pathDecoded[j++] = '.';

            if (path[i] >= 'A' && path[i] <= 'Z')
                pathDecoded[j++] = path[i] - 'A' + 'a';
            else
                pathDecoded[j++] = path[i];
        }
    }
    pathDecoded[j] = '\0';
    return pathDecoded;
}


FAT16 *pre_init_fat16(void)
{
    /* Opening the FAT16 image file */
    FILE *fd;
    FAT16 *fat16_ins;
    fd = fopen(FAT_FILE_NAME, "r+b");
    if (fd == NULL)
    {
        fprintf(stderr, "Missing FAT16 image file!\n");
        exit(EXIT_FAILURE);
    }

    fat16_ins = (FAT16 *)malloc(sizeof(FAT16));
    fat16_ins->fd = fd;

   /** TODO:
    * 初始化fat16_ins的其余成员变量, 该struct定义在fat16.c的第65行
    * 其余成员变量如下:
    *  FirstRootDirSecNum->第一个根目录的扇区偏移.
    *  FirstDataSector->第一个数据区的扇区偏移
    *  Bpb->Bpb结构
    * Hint1: 使用sector_read读出fat16_ins中Bpb项的内容, 这个函数定义在本文件的第18行.
    * Hint2: 可以使用BPB中的参数完成其他变量的初始化, 该struct定义在fat16.c的第23行
    * Hint3: root directory的大小与Bpb.BPB_RootEntCnt有关，并且是扇区对齐的
    **/

//    sector_read(fat16_ins->fd, 0, &fat16_ins->Bpb);
//    fat16_ins->FirstRootDirSecNum = fat16_ins->Bpb.BPB_RsvdSecCnt + fat16_ins->Bpb.BPB_NumFATS /* 2 */ * fat16_ins->Bpb.BPB_FATSz16 /* 21 */ ;
//    // 上取整处理 -1 除 +1
//	DWORD FirstDataSectorTemp = (32 * fat16_ins->Bpb.BPB_RootEntCnt /* 512 */ - 1) / fat16_ins->Bpb.BPB_BytsPerSec /* 512 */ + 1;
//	fat16_ins->FirstDataSector = fat16_ins->FirstRootDirSecNum + FirstDataSectorTemp;

		//read the 0th sector.
    sector_read(fat16_ins->fd, 0, &fat16_ins->Bpb);
    //first root directory sector num
    fat16_ins->FirstRootDirSecNum = fat16_ins->Bpb.BPB_RsvdSecCnt
    + (fat16_ins->Bpb.BPB_FATSz16 * fat16_ins->Bpb.BPB_NumFATS);

		//root directory sectors
    DWORD RootDirSectors = ((fat16_ins->Bpb.BPB_RootEntCnt * 32) +
    (fat16_ins->Bpb.BPB_BytsPerSec - 1)) / fat16_ins->Bpb.BPB_BytsPerSec;
    //data part sectors
    fat16_ins->FirstDataSector = fat16_ins->Bpb.BPB_RsvdSecCnt + (fat16_ins->Bpb.BPB_NumFATS *
    fat16_ins->Bpb.BPB_FATSz16) + RootDirSectors;

    return fat16_ins;
}

/** TODO:
 * 返回簇号为ClusterN对应的FAT表项
 **/
WORD fat_entry_by_cluster(FAT16 *fat16_ins, WORD ClusterN)
{
	BYTE sector_buffer[BYTES_PER_SECTOR];

	WORD offset = ClusterN * 2;

  WORD secNumber = fat16_ins->Bpb.BPB_RsvdSecCnt + (offset / fat16_ins->Bpb.BPB_BytsPerSec);
	WORD tableSize = offset % fat16_ins->Bpb.BPB_BytsPerSec;
	sector_read(fat16_ins->fd, secNumber, &sector_buffer);

	return *((WORD *) &sector_buffer[tableSize]);
}
/**
 *  查询表项值是否为空
 **/
int ClusterEmpty(FAT16 *fat16_ins, WORD ClusterN)
{
	BYTE sector_buffer[BYTES_PER_SECTOR];

	WORD offset = ClusterN * 2;

  WORD secNumber = fat16_ins->Bpb.BPB_RsvdSecCnt + (offset / fat16_ins->Bpb.BPB_BytsPerSec);
	WORD tableSize = offset % fat16_ins->Bpb.BPB_BytsPerSec;
	sector_read(fat16_ins->fd, secNumber, &sector_buffer);

	if( *((WORD *) &sector_buffer[tableSize]) == 0)
	{
		return 0;
	}
	else
		return 1;
}
/*
*  查询第一个空表项
*/
WORD FindEmptyCluster(FAT16 *fat16_ins)
{
	WORD ClusterN = 2;
	WORD tableSize;
	BYTE sector_buffer[BYTES_PER_SECTOR];
	while(ClusterEmpty(fat16_ins, ClusterN))
	{
		ClusterN++;
	}
 return ClusterN;
}
/**
 * 判断是否可以修改簇号为ClusterN对应的FAT表项
 **/
int ClusterIsEmpty(FAT16 *fat16_ins, WORD ClusterN, WORD ClusterValue)
{
	BYTE sector_buffer[BYTES_PER_SECTOR];

	WORD offset = ClusterN * 2;
  WORD secNumber = fat16_ins->Bpb.BPB_RsvdSecCnt + (offset / fat16_ins->Bpb.BPB_BytsPerSec);
	WORD tableSize = offset % fat16_ins->Bpb.BPB_BytsPerSec;
	sector_read(fat16_ins->fd, secNumber, &sector_buffer);

	WORD ClusterNextN = (ClusterValue == 0xffff) ? 0 : fat_entry_by_cluster(fat16_ins, ClusterValue);
	if(ClusterNextN != 0)
		return 1;
	else{
		fseek(fat16_ins->fd, secNumber * fat16_ins->Bpb.BPB_BytsPerSec + tableSize, SEEK_SET);
		int w_size = fwrite(&ClusterValue, sizeof(WORD), 1, fat16_ins->fd);
		fflush(fat16_ins->fd);
		if(ClusterValue == fat_entry_by_cluster(fat16_ins, ClusterN))
			printf("--ISEMPTY: Success!--\n");
		return 0;
	}
}
/**
 * 根据簇号ClusterN，获取其对应的第一个扇区的扇区号和数据，以及对应的FAT表项
 **/
void first_sector_by_cluster(FAT16 *fat16_ins, WORD ClusterN, WORD *FatClusEntryVal, WORD *FirstSectorofCluster, BYTE *buffer)
{

    *FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterN);
    *FirstSectorofCluster = ((ClusterN - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
    sector_read(fat16_ins->fd, *FirstSectorofCluster, buffer);
}

/**
 * 从root directory开始，查找path对应的文件或目录，找到返回0，没找到返回1，并将Dir填充为查找到的对应目录项
 *
 * Hint: 假设path是“/dir1/dir2/file”，则先在root directory中查找名为dir1的目录，
 *       然后在dir1中查找名为dir2的目录，最后在dir2中查找名为file的文件，找到则返回0，并且将file的目录项数据写入到参数Dir对应的DIR_ENTRY中
 **/
int find_root(FAT16 *fat16_ins, DIR_ENTRY *Root, const char *path)
{
    int pathDepth;
    char **paths = path_split((char *)path, &pathDepth);
		printf("--FIND_ROOT: path is %s, paths[0] is %s, paths_last is %s--\n", path, paths[0], paths[pathDepth - 1]);
    /* 先读取root directory */
    int i, j;
    int RootDirCnt = 1;   /* 用于统计已读取的扇区数 */
    int is_eq;
    BYTE buffer[BYTES_PER_SECTOR];

    sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum, buffer);

    /**
     * 查找名字为paths[0]的目录项，
     * 如果找到目录，则根据pathDepth判断是否需要调用find_subdir继续查找，
     *
     * !!注意root directory可能包含多个扇区
     **/
    for (i = 1; i <= fat16_ins->Bpb.BPB_RootEntCnt; i++)
    {
        memcpy(Root, &buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

        /* If the directory entry is free, all the next directory entries are also
         * free. So this file/directory could not be found */

        if (Root->DIR_Name[0] == 0x00)
        {
            return 1;
        }

        /* Comparing strings character by character */
				printf("--FIND_ROOT: paths_last is %s, DIR_Name is %s--\n", paths[pathDepth - 1], Root->DIR_Name);
        is_eq = strncmp(Root->DIR_Name, paths[0], 11) == 0 ? 1 : 0;

        /* If the path is only one file (ATTR_ARCHIVE) and it is located in the
         * root directory, stop searching */
        if (is_eq && Root->DIR_Attr == ATTR_ARCHIVE)
        {
            return 0;
        }

        /* If the path is only one directory (ATTR_DIRECTORY) and it is located in
         * the root directory, stop searching */
        if (is_eq && Root->DIR_Attr == ATTR_DIRECTORY && pathDepth == 1)
        {
            return 0;
        }

        /* If the first level of the path is a directory, continue searching
         * in the root's sub-directories */
        if (is_eq && Root->DIR_Attr == ATTR_DIRECTORY)
        {
            return find_subdir(fat16_ins, Root, paths, pathDepth, 1);
        }

        /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
         * Read next sector */
        if (i % 16 == 0 && i != fat16_ins->Bpb.BPB_RootEntCnt)
        {
            sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum + RootDirCnt, buffer);
            RootDirCnt++;
        }

    }

    return 1;
}

/** TODO:
 * 从子目录开始查找path对应的文件或目录，找到返回0，没找到返回1，并将Dir填充为查找到的对应目录项
 *
 * Hint1: 在find_subdir入口处，Dir应该是要查找的这一级目录的表项，需要根据其中的簇号，读取这级目录对应的扇区数据
 * Hint2: 目录的大小是未知的，可能跨越多个扇区或跨越多个簇；当查找到某表项以0x00开头就可以停止查找
 * Hint3: 需要查找名字为paths[curDepth]的文件或目录，同样需要根据pathDepth判断是否继续调用find_subdir函数
 **/

int find_subdir(FAT16 *fat16_ins, DIR_ENTRY *Dir, char **paths, int pathDepth, int curDepth)
{
    int i, j, SameName;
    int DirSecCnt = 1;
    BYTE buffer[BYTES_PER_SECTOR];
    WORD ClusterN, FatClusEntryVal, FirstSectorofCluster;

    ClusterN = Dir->DIR_FstClusLO;
    FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterN);
    FirstSectorofCluster = ((ClusterN - 2) *fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
    sector_read(fat16_ins->fd, FirstSectorofCluster, buffer);

    //find
    for (i = 1; Dir->DIR_Name[0] != 0x00; i++)
		{
        memcpy(Dir, &buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

        /*比较文件名*/
        SameName = 1;
				printf("--FIND_SUBDIR: dirName is %s, paths[cur] is %s--\n", Dir->DIR_Name, paths[curDepth]);
        for (j = 0; j < 11; j++) {
            if (Dir->DIR_Name[j] != paths[curDepth][j]) {
                SameName = 0;
            break;
        }
    }

    //located successfully
    if ((SameName && Dir->DIR_Attr == ATTR_ARCHIVE && curDepth + 1 == pathDepth) ||
        (SameName && Dir->DIR_Attr == ATTR_DIRECTORY && curDepth + 1 == pathDepth))
		{
        return 0;
    }

    //locate not completed
    if (SameName && Dir->DIR_Attr == ATTR_DIRECTORY)
		{
        return find_subdir(fat16_ins, Dir, paths, pathDepth, curDepth + 1);
    }

    //the end of a sector
    if (i % 16 == 0) {
			//next sector
      if (DirSecCnt < fat16_ins->Bpb.BPB_SecPerClus)
			{
        sector_read(fat16_ins->fd, FirstSectorofCluster + DirSecCnt, buffer);
        DirSecCnt++;
      }
			else
			{
        //next cluster is 0xffff which means the end
        if (FatClusEntryVal == 0xffff)
				{
          return 1;
        }

        //prepare the variable for the next cluster
        ClusterN = FatClusEntryVal;
        FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterN);
        FirstSectorofCluster = ((ClusterN - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
        sector_read(fat16_ins->fd, FirstSectorofCluster, buffer);
        i = 0;
        DirSecCnt = 1;
      }
    }
  }
	//定位ClusterN的位置, DirSecCnt就是扇区数, offset=i%BYTES_PER_SEC
  //located unsuccessfully
  return 1;
}


/** TODO:
 * 从path对应的文件(一定不为根目录"/")的offset字节处开始读取size字节的数据到buffer中，并返回实际读取的字节数
 * Hint: 以扇区为单位读入，需要考虑读到当前簇结尾时切换到下一个簇，并更新要读的扇区号等信息
 * Hint: 文件大小属性是Dir.DIR_FileSize；当offset超过文件大小时，应该返回0
 * 此函数会作为读文件的函数实现被fuse文件系统使用，见fat16_read函数
 * 所以实现正确时的效果为实验文档中的测试二中可以正常访问文件
**/
int read_path(FAT16* fat16_ins, const char *path, size_t size, off_t offset, char *buffer)
{
  int i, j;

  DIR_ENTRY Dir;
  WORD ClusterN, FatClusEntryVal, FirstSectorofCluster;
  BYTE *sector_buffer = malloc((size + offset) * sizeof(BYTE));
  find_root(fat16_ins, &Dir, path);

  //calculate variables
  ClusterN = Dir.DIR_FstClusLO;
  FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterN);
  FirstSectorofCluster = ((ClusterN - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;

  //located
  for (i = 0, j = 0; i < size + offset; i += BYTES_PER_SECTOR, j++)
	{
    sector_read(fat16_ins->fd, FirstSectorofCluster + j, sector_buffer + i);

    //read next cluster
    if ((j + 1) % fat16_ins->Bpb.BPB_SecPerClus == 0)
		{

      //change to next cluster
      ClusterN = FatClusEntryVal;
      FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterN);
      FirstSectorofCluster = ((ClusterN - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
      j = -1;
    }
  }

  //read the file
  if (offset < Dir.DIR_FileSize)
	{
    memcpy(buffer, sector_buffer + offset, size);
  }
  else
	{
    size = 0;
  }

  free(sector_buffer);
  return size;

}
// 从offset字节处, 将size大小的buffer写入进去, 如果空间不足将分配新簇

int write_path(FAT16* fat16_ins, const char *path, size_t size, off_t offset, const char *buffer)
{
  int i, j;
	const int bytes_per_cluster = fat16_ins->Bpb.BPB_SecPerClus * fat16_ins->Bpb.BPB_BytsPerSec;
	int ClusterCountPre = 0, ClusterNPre, ClusterTmp, ClusterLast, ClusterValue, ClusterCount = 1;
  DIR_ENTRY Dir;
  WORD ClusterN, FatClusEntryVal, FirstSectorofCluster;
  BYTE *sector_buffer = malloc((size + offset) * sizeof(BYTE));
  printf("--WRITE_PATH: path is %s--\n", path);
	char *path_pass = (char *)malloc(sizeof(path));
	memcpy(path_pass, path, sizeof(path));
	strcpy(path_pass, path);
  printf("--WRITE_PATH: path_pass is %s--\n", path_pass);

	find_root(fat16_ins, &Dir, path);

  //calculate variables
  ClusterN = Dir.DIR_FstClusLO;
	ClusterNPre = ClusterN;
	ClusterTmp = ClusterN;

	//calculate how many cluster the file has.
	while(ClusterNPre > 1 && ClusterNPre < 0xfff8)
	{
		ClusterCountPre++;
		printf("--WRITE_PATH: ClusterNum is %d--\n", ClusterNPre);
		ClusterTmp = fat_entry_by_cluster(fat16_ins, ClusterNPre);
		if(ClusterTmp <= 1 || ClusterTmp >= 0xfff8)
			ClusterLast = ClusterNPre;
		ClusterNPre = ClusterTmp;
	}

	ClusterValue = ClusterLast;
  FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterLast);
  FirstSectorofCluster = ((ClusterLast - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;

  ClusterCount = (size + offset - 1) / (fat16_ins->Bpb.BPB_SecPerClus * fat16_ins->Bpb.BPB_BytsPerSec) + 1;
	int ClusterRequired = ClusterCount - ClusterCountPre;
	if(ClusterRequired > 0)
	{
			int b = 1;
			for(int a = 1; a <= ClusterRequired; a++)
			{
				b = 1;
				while(ClusterIsEmpty(fat16_ins, ClusterLast, ClusterLast + b))
					b++;
				printf("--WRITE_PATH: Cluster Num. is %d--\n", ClusterLast);
				ClusterLast = fat_entry_by_cluster(fat16_ins, ClusterLast);
				printf("--WRITE_PATH: Cluster Num. After is %d--\n", ClusterLast);
				if(a == ClusterRequired)
				{
					ClusterIsEmpty(fat16_ins, ClusterLast, 0xffff);
				}
			}
	}

  FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterValue);
  FirstSectorofCluster = ((ClusterValue - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
	printf("--WRITE_PATH: step1--\n");
  //located and writed
	int ClustCount = 1, length = 0;
	off_t offset_temp = offset % bytes_per_cluster;
	off_t offset_temp2 = offset;
	size_t size_temp = size;
	printf("--WRITE_PATH: offset is %ld, offset_temp is %ld, size is %ld\n", offset, offset_temp, size);
	if(ClusterRequired > 0)
	{
		int ClustReqCnt;
		int size_offset = size + offset;
		FILE *fd = fat16_ins->fd;
		DIR_ENTRY Dir;
		find_root(fat16_ins, &Dir, path);
		if(offset_temp == 0)
		{
			ClustReqCnt = (size - 1) / bytes_per_cluster + 1;
			if(Dir.DIR_FileSize != 0)
			{
				ClusterValue = fat_entry_by_cluster(fat16_ins, ClusterValue);
  			FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterValue);
  			FirstSectorofCluster = ((ClusterValue - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
			}
			else
				;
		}
		else
		{
			size -= bytes_per_cluster - offset_temp;
			ClustReqCnt = (size - 1) / bytes_per_cluster + 1;

			fd = fat16_ins->fd;
			length = bytes_per_cluster - offset_temp;
			fseek(fd, FirstSectorofCluster * fat16_ins->Bpb.BPB_BytsPerSec + offset_temp, SEEK_SET);
			printf("--WRITE_PATH: step2----current Cluster is %d--\n", ClusterValue);
			int w_size = fwrite((char *)buffer, sizeof(char), length, fat16_ins->fd);
			buffer += length;
			edit_size(path_pass, offset_temp2 += length);
    	printf("--WRITE_PATH: filesize after is %d--\n", offset_temp2);
			printf("--WRITE_PATH: step0.0----Change Cluster Num from %d to %d\n--", ClusterValue, FatClusEntryVal);
      ClusterValue = FatClusEntryVal;
      FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterValue);
      FirstSectorofCluster = ((ClusterValue - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
		}
		for (i = 0; i < ClustReqCnt; i++)
		{
			fd = fat16_ins->fd;
			length = (i == ClustReqCnt - 1) ? size % bytes_per_cluster : bytes_per_cluster;
			fseek(fd, FirstSectorofCluster * fat16_ins->Bpb.BPB_BytsPerSec, SEEK_SET);
			int w_size = fwrite((char *)buffer,  sizeof(char), length, fat16_ins->fd);
			buffer += length;
			fflush(fd);

			edit_size(path_pass, offset_temp2 += length);
    	printf("--WRITE_PATH: filesize after is %d--\n", offset_temp2);
			//read next cluster

			printf("--WRITE_PATH: step0.0----Change Cluster Num from %d to %d\n--", ClusterValue, FatClusEntryVal);
      ClusterValue = FatClusEntryVal;
      FatClusEntryVal = fat_entry_by_cluster(fat16_ins, ClusterValue);
      FirstSectorofCluster = ((ClusterValue - 2) * fat16_ins->Bpb.BPB_SecPerClus) + fat16_ins->FirstDataSector;
			ClustCount++;
  	}
	}
	else// no new cluster required
	{
			FILE *fd = fat16_ins->fd;
			length = size;
			fseek(fd, FirstSectorofCluster * fat16_ins->Bpb.BPB_BytsPerSec + offset_temp, SEEK_SET);
			printf("--WRITE_PATH: step2----current Cluster is %d--\n", ClusterValue);
			int w_size = fwrite((char *)buffer, sizeof(char), length, fat16_ins->fd);
			buffer += length;
			edit_size(path_pass, offset_temp2 += length);
    	printf("--WRITE_PATH: filesize after is %d--\n", offset_temp2);
	}
  return size_temp;

}

/**
 * ------------------------------------------------------------------------------
 * FUSE相关的函数实现
 **/

/* Function: plit the path, while keep their original format
 * ==================================================================================
 * exp: "/dir1/dir2/text"  -> {"dir1","dir2","text"}
 * ==================================================================================
*/
char **org_path_split(char *pathInput){
  int i, j;
  int pathDepth = 0;
  for (i = 0; pathInput[i] != '\0'; i++)
  {
    if (pathInput[i] == '/')
    {
      pathDepth++;
    }
  }
	printf("--ORG_PATH_SPLIT: pathDepth is %d--\n", pathDepth);
	char *path_const = malloc((i + 1) * sizeof(char));
	strcpy(path_const, pathInput);
  char **orgPaths = (char **)malloc(pathDepth * sizeof(char *));
  const char token[] = "/";
  char *slice;

  /* Dividing the path into separated strings of file names */
  slice = strtok(path_const, token);
  for (i = 0; i < pathDepth; i++)
  {
    orgPaths[i] = slice;
    slice = strtok(NULL, token);
  }
	for(i = 0; i < pathDepth; i++)
	{
		printf("--ORG_PATH_SPLIT: Num %d is %s--\n", i, orgPaths[i]);
	}
  return orgPaths;
}


/* Function: Get parent directory path of a specified file
 * ==================================================================================
 * exp: path = "dir1/dir2/texts" orgPaths = { "dir1", "dir2", "tests" }
 * Return "dir1/dir2"
 * ==================================================================================
*/
char * get_prt_path(const char *path, const char **orgPaths,int pathDepth){
  char *prtPath;
	int prtPathLen;
  if(pathDepth == 1){
    prtPath = (char *)malloc(2*sizeof(char));
    prtPath[0] = '/';
    prtPath[1] = '\0';
  }
  else {
    prtPathLen = strlen(path) - strlen(orgPaths[pathDepth-1])-1;
		printf("--GET_PRT_PATH: path is %s - %d, orgPaths[pathDepth-1] is %s - %d--\n", path, strlen(path), orgPaths[pathDepth-1], strlen(orgPaths[pathDepth - 1]));
    prtPath = (char *)malloc((prtPathLen+1)*sizeof(char));
    strncpy(prtPath, path, prtPathLen);
    prtPath[prtPathLen] = '\0';
  }
	printf("--GET_PRT_PATH: prtPathLen is %d, prtPath is %s--\n", prtPathLen, prtPath);
  return prtPath;
}

void *fat16_init(struct fuse_conn_info *conn)
{
    struct fuse_context *context;
    context = fuse_get_context();

    return context->private_data;
}

void fat16_destroy(void *data)
{
    free(data);
}

int fat16_getattr(const char *path, struct stat *stbuf)
{
    FAT16 *fat16_ins;

    struct fuse_context *context;
    context = fuse_get_context();
    fat16_ins = (FAT16 *)context->private_data;

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_dev = fat16_ins->Bpb.BS_VollID;
    stbuf->st_blksize = BYTES_PER_SECTOR * fat16_ins->Bpb.BPB_SecPerClus;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | S_IRWXU;
        stbuf->st_size = 0;
        stbuf->st_blocks = 0;
        stbuf->st_ctime = stbuf->st_atime = stbuf->st_mtime = 0;
    }
    else
    {
        DIR_ENTRY Dir;

        int res = find_root(fat16_ins, &Dir, path);

        if (res == 0)
        {
            if (Dir.DIR_Attr == ATTR_DIRECTORY)
            {
                stbuf->st_mode = S_IFDIR | 0755;
            }
            else
            {
                stbuf->st_mode = S_IFREG | 0755;
            }
            stbuf->st_size = Dir.DIR_FileSize;

            if (stbuf->st_size % stbuf->st_blksize != 0)
            {
                stbuf->st_blocks = (int)(stbuf->st_size / stbuf->st_blksize) + 1;
            }
            else
            {
                stbuf->st_blocks = (int)(stbuf->st_size / stbuf->st_blksize);
            }

            struct tm t;
            memset((char *)&t, 0, sizeof(struct tm));
            t.tm_sec = Dir.DIR_WrtTime & ((1 << 5) - 1);
            t.tm_min = (Dir.DIR_WrtTime >> 5) & ((1 << 6) - 1);
            t.tm_hour = Dir.DIR_WrtTime >> 11;
            t.tm_mday = (Dir.DIR_WrtDate & ((1 << 5) - 1));
            t.tm_mon = (Dir.DIR_WrtDate >> 5) & ((1 << 4) - 1);
            t.tm_year = (Dir.DIR_WrtDate >> 9) + 80;
            stbuf->st_ctime = stbuf->st_atime = stbuf->st_mtime = mktime(&t);
        }
        else return -ENOENT;
    }
    return 0;
}

int fat16_utimens(const char * path, const struct timespec tv[2])
{
	return 0;
}
int fat16_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi)
{
    FAT16 *fat16_ins;
    BYTE sector_buffer[BYTES_PER_SECTOR];
    int RootDirCnt = 1, DirSecCnt = 1, i;

    struct fuse_context *context;
    context = fuse_get_context();
    fat16_ins = (FAT16 *)context->private_data;
		printf("--READDIR: secnum is %ld--\n", fat16_ins->FirstRootDirSecNum);
    sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum, sector_buffer);

    if (strcmp(path, "/") == 0)
    {
        DIR_ENTRY Root;
        for (i = 1; i <= fat16_ins->Bpb.BPB_RootEntCnt; i++)
        {
            memcpy(&Root, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

            // No more files to fill
            if (Root.DIR_Name[0] == 0x00)
            {
                return 0;
            }

            // If we find a file or a directory, fill it into the buffer
            if ((Root.DIR_Attr == ATTR_ARCHIVE || Root.DIR_Attr == ATTR_DIRECTORY) && Root.DIR_Name[0] != 0xe5)
            {
                const char *filename = (const char *)path_decode(Root.DIR_Name);
                filler(buffer, filename, NULL, 0);
            }


            // Read next sector
            if (i % 16 == 0 && i != fat16_ins->Bpb.BPB_RootEntCnt)
            {
                sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum + RootDirCnt, sector_buffer);
                RootDirCnt++;
            }
        }
    }
    else
    {
        DIR_ENTRY Dir;
        int res = find_root(fat16_ins, &Dir, path);
        if (res == 1)
            return -ENOENT;

        WORD ClusterN, FatClusEntryVal, FirstSectorofCluster;
        ClusterN = Dir.DIR_FstClusLO;
        first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
        for (i = 1; Dir.DIR_Name[0] != 0x00; i++)
        {
            memcpy(&Dir, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

            if ((Dir.DIR_Attr == ATTR_ARCHIVE || Dir.DIR_Attr == ATTR_DIRECTORY) && Dir.DIR_Name[0] != 0xe5)
            {
                const char *filename = (const char *)path_decode(Dir.DIR_Name);
                filler(buffer, filename, NULL, 0);
            }

            if (i % 16 == 0)
            {
                if (DirSecCnt < fat16_ins->Bpb.BPB_SecPerClus)
                {
                    sector_read(fat16_ins->fd, FirstSectorofCluster + DirSecCnt, sector_buffer);
                    DirSecCnt++;
                }
                else
                {
                    if (FatClusEntryVal == 0xffff)
                    {
                        return 0;
                    }
                    // Next cluster
                    ClusterN = FatClusEntryVal;
                    first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
                    i = 0;
                    DirSecCnt = 1;
                }
            }
        }
    }
    return 0;
}


/* 读文件接口，直接调用read_path实现 */
int fat16_read(const char *path, char *buffer, size_t size, off_t offset,
        struct fuse_file_info *fi)
{
    FAT16 *fat16_ins;
    struct fuse_context *context;
    context = fuse_get_context();
    fat16_ins = (FAT16 *)context->private_data;

    return read_path(fat16_ins, path, size, offset, buffer);
}

/* Fat16 Create Files Functions */
/** TODO:
 * 装填条目
 * 填入相应的属性值
 * 已经给出了文件名的示例，其它域由你们完成
 **/
int dir_entry_create(FAT16 *fat16_ins,int sectorNum,int offset,char *Name, BYTE attr, WORD firstClusterNum,DWORD fileSize){
  /* Create memory buffer to store entry info */
  BYTE *entry_info = malloc(BYTES_PER_DIR*sizeof(BYTE));
  /* Fill in file name */

  time_t timer_s;
  time(&timer_s);
  struct tm *time_ptr = localtime(&timer_s);
  int value;

	WORD update_time = time_ptr->tm_hour * 2048 + time_ptr->tm_min * 32 + time_ptr->tm_sec / 2;
	WORD update_year = (time_ptr->tm_year - 2000) * 512 + time_ptr->tm_mon * 32 + time_ptr->tm_mday;
	printf("--CREATE: update_time is %ld, update_year is %ld--\n", update_time, update_year);

  memcpy(entry_info, Name, 11);
	memcpy(entry_info + 11, &attr, 1);
  memset(entry_info + 12, 0, 10 * sizeof(BYTE));
	memcpy(entry_info + 22, &update_time, 2);
	memcpy(entry_info + 24, &update_year, 2);
	memcpy(entry_info + 26, &firstClusterNum, 2);
	memcpy(entry_info + 28, &fileSize, 4);

  /* Write the above entry to specified location */
  FILE *fd = fat16_ins->fd;
  BYTE *bufferm = malloc(BYTES_PER_DIR*sizeof(BYTE));
	printf("--CREATE: attr is %lx, sectorNum is %ld, offset is %ld--\n", attr, sectorNum, offset);
	fseek(fd, sectorNum * fat16_ins->Bpb.BPB_BytsPerSec + offset, SEEK_SET);

	int w_size = fwrite(entry_info, sizeof(BYTE), 32, fd);
	if(w_size == NULL)
		perror("--CREATE ERROR");
	fflush(fd);
  free(entry_info);
  return 0;
}

int fat16_mknod(const char *path, mode_t mode, dev_t devNum)
{
  FAT16 *fat16_ins;
  struct fuse_context *context;
  context = fuse_get_context();
  fat16_ins = (FAT16 *)context->private_data;
  /** TODO:
   * 查找新建文件的父目录，你可以使用辅助函数org_path_split和get_prt_path
   **/

	char * path_const = (char *)malloc(sizeof(path));
	memcpy(path_const, path, sizeof(path));
	strcpy(path_const, path);
	char * path_const2 = (char *)malloc(sizeof(path));
	memcpy(path_const2, path, sizeof(path));
	strcpy(path_const2, path);
	char * path_const3 = (char *)malloc(sizeof(path));
	memcpy(path_const3, path, sizeof(path));
	strcpy(path_const3, path);
	int pathDepth = depth(path_const);
	char **pathSplit = org_path_split(path_const);
	char *prtPath = get_prt_path(path_const2, pathSplit, pathDepth);
	printf("--MKNOD: Path Before is %s--\n", path);
	printf("--MKNOD: pathDepth is %ld, pathSplit[0] is %s,pathSplit[pathDepth - 1] is %s, prtPath is %s--\n", pathDepth, pathSplit[0], pathSplit[pathDepth - 1], prtPath);

  /** TODO:
   * 查找可用的entry，注意区分根目录和子目录
   * 下面提供了一些可能使用到的临时变量
   * 如果觉得不够用，可以自己定义更多的临时变量
   * 这块和前面有很多相似的地方，注意对照来实现
   **/
  BYTE sector_buffer[BYTES_PER_SECTOR];
  DWORD sectorNum;
  int offset, i,findFlag = 0, RootDirCnt = 1, DirSecCnt = 1, ClusterCnt = 0;
  WORD ClusterN, FatClusEntryVal, FirstSectorofCluster;

  if (strcmp(prtPath, "/") == 0){
    DIR_ENTRY Root;
    sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum, sector_buffer);

		for(i = 1; i <= fat16_ins->Bpb.BPB_RootEntCnt; i++)
		{
			memcpy(&Root, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);
			if(Root.DIR_Name[0] == 0x00 || Root.DIR_Name[0] == 0xe5) // if encountered a deleted directory, cover it.
			{
				printf("The dir name is %s\n", Root.DIR_Name);
				//calculate variable of the location of empty entry.
				sectorNum = RootDirCnt - 1 + fat16_ins->Bpb.BPB_RsvdSecCnt + fat16_ins->Bpb.BPB_NumFATS * fat16_ins->Bpb.BPB_FATSz16;
				offset = ((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR;
				findFlag = 1;
				break;
			}
			if(i % 16 == 0 && i != fat16_ins->Bpb.BPB_RootEntCnt)
			{
				sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum + RootDirCnt, sector_buffer);
				RootDirCnt++;
			}

		}
  }

  else{
		findFlag = 1;
    DIR_ENTRY Dir;
		printf("--MKNOD: Prtpath is %s\n", prtPath);

    if(find_root(fat16_ins,&Dir,prtPath))
			return -ENOENT;

    ClusterN = Dir.DIR_FstClusLO;
    first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);

		for(i = 1; (Dir.DIR_Name[0] != 0x00 && Dir.DIR_Name[0] != 0xe5); i++)
		{
			memcpy(&Dir, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);
			if(i % 16 == 0)
			{
				if (DirSecCnt < fat16_ins->Bpb.BPB_SecPerClus)
        {
        	sector_read(fat16_ins->fd, FirstSectorofCluster + DirSecCnt, sector_buffer);
          DirSecCnt++;
        }
				else
				{
					if(FatClusEntryVal == 0xffff)
					{
						findFlag = 0;
						break;
					}
					ClusterN = FatClusEntryVal;
					first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
					ClusterCnt++;
					i = 0;
					DirSecCnt = 1;
				}
			}

		}
		//calculate variables of location of empty entry.
		sectorNum = DirSecCnt - 1 +
					(ClusterN - 2) * fat16_ins->Bpb.BPB_SecPerClus +
								fat16_ins->FirstDataSector;
		offset = ((i - 2) * BYTES_PER_DIR) % BYTES_PER_SECTOR;
  }
	printf("--MKNOD: sectorNum is %d, offset is %d--\n", sectorNum, offset);

  if(findFlag == 1)
	{
		int j = 0;
		char **path_test = path_split(path_const3, &j);
		DIR_ENTRY test;
		WORD Cluster_temp = FindEmptyCluster(fat16_ins);
		printf("--MKNOD: path_test[0] is %s--\n", path_test[0]);
		printf("--MKNOD: Name is %s, attr is %ld, firstClusterNum is %ld, fileSize is %ld--\n", path_test[j - 1], ATTR_ARCHIVE, Cluster_temp, 0);
		printf("--MKNOD: paths[pathDepth - 1] is %s--\n", pathSplit[pathDepth - 1]);
		dir_entry_create(fat16_ins, sectorNum, offset, path_test[j - 1], ATTR_ARCHIVE, Cluster_temp, 0);
		ClusterIsEmpty(fat16_ins, Cluster_temp, 0xffff);


		BYTE buffer_test[BYTES_PER_SECTOR];
    sector_read(fat16_ins->fd, sectorNum, buffer_test);
		memcpy(&test, &buffer_test[offset], BYTES_PER_DIR);
    int time1 = test.DIR_WrtTime & ((1 << 5) - 1);
    int time2 = (test.DIR_WrtTime >> 5) & ((1 << 6) - 1);
    int time3 = test.DIR_WrtTime >> 11;
    int time_mday = (test.DIR_WrtDate & ((1 << 5) - 1));
    int time_mon = (test.DIR_WrtDate >> 5) & ((1 << 4) - 1);
    int time_year = 80 + (test.DIR_WrtDate >> 9);
		printf("--MKNOD: test Name is %s, Attr is %ld, firstClusterNum is %ld, time is %ld\ntime is %d:%d:%d \ndate is %d/%d/%d --\n", test.DIR_Name, test.DIR_Attr, test.DIR_FstClusLO, test.DIR_WrtTime, time3, time2, time1, time_year+1900, time_mon, time_mday);
	}
	printf("--WARNING: MKNOD: path is %s--\n", path);
	return 0;
}

/**
 * free cluster 时，只修改FAT对应表项
 * @return 下一个簇的簇号
  */
int freeCluster(FAT16 *fat16_ins, int ClusterNum){
  BYTE sector_buffer[BYTES_PER_SECTOR];
  WORD FATClusEntryval, FirstSectorofCluster, ClusterN;
  first_sector_by_cluster(fat16_ins,ClusterNum,&FATClusEntryval,&FirstSectorofCluster,sector_buffer);

  FILE *fd = fat16_ins->fd;
  /** TODO:
   * 修改FAT表
   * 注意两个表都要修改
   **/
   /* 代码开始 */
	ClusterN = (WORD)ClusterNum;
	WORD FAT_offset = ClusterN * 2;
	WORD FatEntSize = FAT_offset % fat16_ins->Bpb.BPB_BytsPerSec;
	WORD FatSecNum1 = fat16_ins->Bpb.BPB_RsvdSecCnt + (FAT_offset / fat16_ins->Bpb.BPB_BytsPerSec);
	WORD FatSecNum2 = fat16_ins->Bpb.BPB_RsvdSecCnt +
													fat16_ins->Bpb.BPB_FATSz16 + (FAT_offset / fat16_ins->Bpb.BPB_BytsPerSec);

	WORD temp = 0;
	fseek(fd, FatSecNum1 * fat16_ins->Bpb.BPB_BytsPerSec + FatEntSize, SEEK_SET);
  int w_size_1 = fwrite(&temp, sizeof(WORD), 1, fd);
  fflush(fd);

	fd = fat16_ins->fd;
	fseek(fd, FatSecNum2 * fat16_ins->Bpb.BPB_BytsPerSec + FatEntSize, SEEK_SET);
  int w_size_2 = fwrite(&temp, sizeof(WORD), 1, fd);
  fflush(fd);

  return FATClusEntryval;
}

/* Function: remove a file */
int fat16_unlink(const char *path){
  /* Gets volume data supplied in the context during the fat16_init function */
  FAT16 *fat16_ins;
  struct fuse_context *context;
  context = fuse_get_context();
  fat16_ins = (FAT16 *)context->private_data;
	printf("--UNLINK: path is %s--\n", path);

	char * path_const = (char *)malloc(sizeof(path));
	memcpy(path_const, path, sizeof(path));
	strcpy(path_const, path);
	char * path_const2 = (char *)malloc(sizeof(path));
	memcpy(path_const2, path, sizeof(path));
	strcpy(path_const2, path);

  /** TODO:
   * 回收该文件所占有的簇
   * 注意完善并使用freeCluster函数
   **/
  WORD ClusterN, N;
  DIR_ENTRY Dir;
  //释放使用过的簇
  if(find_root(fat16_ins, &Dir, path_const) == 1){
    return 1;
  }
	printf("--UNLINK: path after is %s--\n", path);
	printf("--UNLINK: Dir.Name is %s--\n", Dir.DIR_Name);
  ClusterN = Dir.DIR_FstClusLO;
  /* 代码开始 */
	do
	{
		ClusterN = freeCluster(fat16_ins, ClusterN);
		printf("--FREECLU: ClusterN is %lx--\n", ClusterN);
	}
	while(ClusterN > 1 && ClusterN < 0xFFF8);

  /* Find the location(sector number & offset) of file entry */
  /** TODO:
   * 查找新建文件的父目录，你可以使用辅助函数org_path_split和get_prt_path
   * 这部分内容和mknode函数差不多
   **/

	int pathDepth = depth(path_const2);
	char **pathSplit = org_path_split(path_const2);
	char *prtPath = get_prt_path(path, pathSplit, pathDepth);
	printf("--UNLINK: Path Before is %s--\n", path);
	printf("--UNLINK: pathDepth is %ld, pathSplit[0] is %s,pathSplit[pathDepth - 1] is %s, prtPath is %s--\n", pathDepth, pathSplit[0], pathSplit[pathDepth - 1], prtPath);

  /** TODO:
   * 定位文件在父目录中的entry，注意区分根目录和子目录
   * 下面提供了一些可能使用到的临时变量
   * 如果觉得不够用，可以自己定义更多的临时变量
   * 这块和前面有很多相似的地方，注意对照来实现
   * 流程类似，大量代码都和mknode一样，注意复用
   **/
  BYTE sector_buffer[BYTES_PER_SECTOR];
  DWORD sectorNum;
  int offset, i,findFlag = 0, RootDirCnt = 1, DirSecCnt = 1;
  WORD FatClusEntryVal, FirstSectorofCluster;

  if (strcmp(prtPath, "/") == 0){
    DIR_ENTRY Root;
    sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum, sector_buffer);

		for(i = 1; i <= fat16_ins->Bpb.BPB_RootEntCnt; i++)
		{
			memcpy(&Root, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);
			printf("--UNLINK: Dir_name is %s, pathSplit[dep-1] is %s--\n", path_decode(Root.DIR_Name), pathSplit[pathDepth - 1]);
			if(strcmp(path_decode(Root.DIR_Name), pathSplit[pathDepth - 1]) == 0)
			{
				sectorNum = RootDirCnt - 1 + fat16_ins->Bpb.BPB_RsvdSecCnt + fat16_ins->Bpb.BPB_NumFATS * fat16_ins->Bpb.BPB_FATSz16;
				offset = ((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR;
				findFlag = 1;
				break;
			}

			if(i % 16 == 0 && i != fat16_ins->Bpb.BPB_RootEntCnt)
			{
				sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum + RootDirCnt, sector_buffer);
				RootDirCnt++;
			}

		}
  }

  else{
    DIR_ENTRY Dir;
    find_root(fat16_ins,&Dir,prtPath);
    ClusterN = Dir.DIR_FstClusLO;
    first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);

		findFlag = 0;
		for(i = 1; Dir.DIR_Name[0] != 0x00; i++)
		{
			memcpy(&Dir, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);
			printf("--UNLINK: Dir_Name is %s, pathSplit[dep-1] is %s--\n", path_decode(Dir.DIR_Name), pathSplit[pathDepth - 1]);
			if(strcmp(path_decode(Dir.DIR_Name), pathSplit[pathDepth - 1]) == 0)
			{
				findFlag = 1;
				sectorNum = DirSecCnt - 1 +
							(ClusterN - 2) * fat16_ins->Bpb.BPB_SecPerClus +
										fat16_ins->FirstDataSector;
				offset = ((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR;
				break;
			}
			if(i % 16 == 0)
			{
				if (DirSecCnt < fat16_ins->Bpb.BPB_SecPerClus)
        {
        	sector_read(fat16_ins->fd, FirstSectorofCluster + DirSecCnt, sector_buffer);
          DirSecCnt++;
        }
				else
				{
					if(FatClusEntryVal == 0xffff)
					{
						findFlag = 0;
						break;
					}
					ClusterN = FatClusEntryVal;
					first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
					i = 0;
					DirSecCnt = 1;
				}
			}

		}
  }

  /** TODO:
   * 删除文件，对相应entry做标记
   * 思考要修改entry中的哪些域
   **/

  /* Update file entry, change its first byte of file name to 0xe5 */
  if (findFlag == 1)
	{
    FILE *fd = fat16_ins->fd;
		BYTE empty_cover = 0xe5;
		DIR_ENTRY test;
		BYTE buffer_test[BYTES_PER_SECTOR];

		fseek(fd, sectorNum * fat16_ins->Bpb.BPB_BytsPerSec + offset, SEEK_SET);
  	int w_size_de = fwrite(&empty_cover, sizeof(BYTE), 1, fd);
    sector_read(fat16_ins->fd, sectorNum, buffer_test);
		memcpy(&test, &buffer_test[offset], BYTES_PER_DIR);
		printf("--UNLINK: name is %s, first ascii is %d--\n", test.DIR_Name, test.DIR_Name[0]);

    fflush(fd);
  }
  return 0;
}

/* read_path()调试思路参考，不作为计分标准 */
void test_read_path() {
    char *buffer;
    /* 这里使用的文件镜像是fat16_test.img */
    FAT_FILE_NAME = "fat16_test.img";
    FAT16 *fat16_ins = pre_init_fat16();

    /* 测试用的文件、读取大小、位移和应该读出的文本 */
    char path[][32] = {"/Makefile", "/log.c", "/dir1/dir2/dir3/test.c"};
    int size[] = {32, 9, 9};
    int offset[] = {8, 9, 9};
    char texts[][32] = {"(shell pkg-config fuse --cflags)", "<errno.h>", "<errno.h>"};

    int i;
    for (i = 0; i < sizeof(path) / sizeof(path[0]); i++) {
        DIR_ENTRY Dir;
        buffer = malloc(size[i]*sizeof(char));
        /* 读文件 */
        read_path(fat16_ins, path[i], size[i], offset[i], buffer);
        /* 比较读出的结果 */
        assert(strncmp(buffer, texts[i], size[i]) == 0);
        free(buffer);
        printf("test case %d: OK\n", i + 1);
    }

    fclose(fat16_ins->fd);
    free(fat16_ins);
}
int edit_size(const char *path, DWORD filesize){
  /* Gets volume data supplied in the context during the fat16_init function */
  FAT16 *fat16_ins;
  struct fuse_context *context;
  context = fuse_get_context();
  fat16_ins = (FAT16 *)context->private_data;
	printf("--EDIT_SIZE: path is %s--\n", path);

	char * path_const = (char *)malloc(sizeof(path));
	memcpy(path_const, path, sizeof(path));
	strcpy(path_const, path);
	char * path_const2 = (char *)malloc(sizeof(path));
	memcpy(path_const2, path, sizeof(path));
	strcpy(path_const2, path);

  /** TODO:
   * 回收该文件所占有的簇
   * 注意完善并使用freeCluster函数
   **/
  WORD ClusterN, N;
  //释放使用过的簇
	printf("--EDIT_SIZE: path after is %s--\n", path);
  /* 代码开始 */

  /* Find the location(sector number & offset) of file entry */
  /** TODO:
   * 查找新建文件的父目录，你可以使用辅助函数org_path_split和get_prt_path
   * 这部分内容和mknode函数差不多
   **/

	int pathDepth = depth(path_const2);
	char **pathSplit = org_path_split(path_const2);
	char *prtPath = get_prt_path(path, pathSplit, pathDepth);
	printf("--EDIT_SIZE: Path Before is %s--\n", path);
	printf("--EDIT_SIZE: pathDepth is %ld, pathSplit[0] is %s,pathSplit[pathDepth - 1] is %s, prtPath is %s--\n", pathDepth, pathSplit[0], pathSplit[pathDepth - 1], prtPath);

  /** TODO:
   * 定位文件在父目录中的entry，注意区分根目录和子目录
   * 下面提供了一些可能使用到的临时变量
   * 如果觉得不够用，可以自己定义更多的临时变量
   * 这块和前面有很多相似的地方，注意对照来实现
   * 流程类似，大量代码都和mknode一样，注意复用
   **/
  BYTE sector_buffer[BYTES_PER_SECTOR];
  DWORD sectorNum;
  int offset, i,findFlag = 0, RootDirCnt = 1, DirSecCnt = 1;
  WORD FatClusEntryVal, FirstSectorofCluster;

  if (strcmp(prtPath, "/") == 0){
    DIR_ENTRY Root;
    sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum, sector_buffer);

		for(i = 1; i <= fat16_ins->Bpb.BPB_RootEntCnt; i++)
		{
			memcpy(&Root, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);
			printf("--EDIT_SIZE: Dir_name is %s, pathSplit[dep-1] is %s--\n", path_decode(Root.DIR_Name), pathSplit[pathDepth - 1]);
			if(strcmp(path_decode(Root.DIR_Name), pathSplit[pathDepth - 1]) == 0)
			{
				sectorNum = RootDirCnt - 1 + fat16_ins->Bpb.BPB_RsvdSecCnt + fat16_ins->Bpb.BPB_NumFATS * fat16_ins->Bpb.BPB_FATSz16;
				offset = ((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR;
				findFlag = 1;
				break;
			}

			if(i % 16 == 0 && i != fat16_ins->Bpb.BPB_RootEntCnt)
			{
				sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum + RootDirCnt, sector_buffer);
				RootDirCnt++;
			}

		}
  }

  else{
    DIR_ENTRY Dir;
    find_root(fat16_ins,&Dir,prtPath);
    ClusterN = Dir.DIR_FstClusLO;
    first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);

		findFlag = 0;
		for(i = 1; Dir.DIR_Name[0] != 0x00; i++)
		{
			memcpy(&Dir, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);
			printf("--EDIT_SIZE: Dir_Name is %s, pathSplit[dep-1] is %s--\n", path_decode(Dir.DIR_Name), pathSplit[pathDepth - 1]);
			if(strcmp(path_decode(Dir.DIR_Name), pathSplit[pathDepth - 1]) == 0)
			{
				findFlag = 1;
				sectorNum = DirSecCnt - 1 +
							(ClusterN - 2) * fat16_ins->Bpb.BPB_SecPerClus +
										fat16_ins->FirstDataSector;
				offset = ((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR;
				break;
			}
			if(i % 16 == 0)
			{
				if (DirSecCnt < fat16_ins->Bpb.BPB_SecPerClus)
        {
        	sector_read(fat16_ins->fd, FirstSectorofCluster + DirSecCnt, sector_buffer);
          DirSecCnt++;
        }
				else
				{
					if(FatClusEntryVal == 0xffff)
					{
						findFlag = 0;
						break;
					}
					ClusterN = FatClusEntryVal;
					first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
					i = 0;
					DirSecCnt = 1;
				}
			}

		}
  }

  /* Update file entry, change its first byte of file name to 0xe5 */
  if (findFlag == 1)
	{
    FILE *fd = fat16_ins->fd;
		DIR_ENTRY test;
		BYTE buffer_test[BYTES_PER_SECTOR];

    sector_read(fat16_ins->fd, sectorNum, buffer_test);
		memcpy(&test, &buffer_test[offset], BYTES_PER_DIR);
		printf("--EDIT_SIZE: before filesize is %d--\n", test.DIR_FileSize);
		fseek(fd, sectorNum * fat16_ins->Bpb.BPB_BytsPerSec + offset + 28, SEEK_SET);
  	int w_size = fwrite(&filesize, sizeof(BYTE), 4, fd);
    sector_read(fat16_ins->fd, sectorNum, buffer_test);
		memcpy(&test, &buffer_test[offset], BYTES_PER_DIR);
		printf("--EDIT_SIZE: after filesize is %d--\n", test.DIR_FileSize);
		printf("--EDIT_SIZE: bytes per cluster is %d--\n", fat16_ins->Bpb.BPB_BytsPerSec * fat16_ins->Bpb.BPB_SecPerClus);

    fflush(fd);
  }
  return 0;
}
int fat16_write(const char * path, const char * buffer, size_t size, off_t offset, struct fuse_file_info * fi)
{
		// 1. 找到文件
		// 2. 找到文件初始簇号
		// 3. 往初始簇号写内容
		// 4. 如果不够, 分配新簇号
		// 5. 往新簇号写内容

    FAT16 *fat16_ins;
    struct fuse_context *context;
    context = fuse_get_context();
    fat16_ins = (FAT16 *)context->private_data;

    return write_path(fat16_ins, path, size, offset, buffer);
}
//add create and delete
struct fuse_operations fat16_oper = {
    .init = fat16_init,
    .destroy = fat16_destroy,
    .getattr = fat16_getattr,
    .readdir = fat16_readdir,
    .read = fat16_read,
    .mknod = fat16_mknod,
    .unlink = fat16_unlink,
		.utimens = fat16_utimens,
		.write = fat16_write
};

int main(int argc, char *argv[])
{
    int ret;

    if (strcmp(argv[1], "--test") == 0) {
        printf("--------------\nrunning test\n--------------\n");
        FAT_FILE_NAME = "fat16_test.img";
        test_path_split();
        test_pre_init_fat16();
        test_fat_entry_by_cluster();
        test_find_subdir();
        exit(EXIT_SUCCESS);
    }

    FAT16 *fat16_ins = pre_init_fat16();

    ret = fuse_main(argc, argv, &fat16_oper, fat16_ins);

    return ret;
}
