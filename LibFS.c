#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "LibDisk.h"
#include "LibFS.h"

// set to 1 to have detailed debug print-outs and 0 to have none
#define FSDEBUG 0

#if FSDEBUG
#define dprintf printf
#else
#define dprintf noprintf
void noprintf(char* str, ...) {}
#endif


// the file system partitions the disk into five parts:

// 1. the superblock (one sector), which contains a magic number at
// its first four bytes (integer)
#define SUPERBLOCK_START_SECTOR 0

// the magic number chosen for our file system
#define OS_MAGIC 0xdeadbeef

// 2. the inode bitmap (one or more sectors), which indicates whether
// the particular entry in the inode table (#4) is currently in use
#define INODE_BITMAP_START_SECTOR 1

// the total number of bytes and sectors needed for the inode bitmap;
// we use one bit for each inode (whether it's a file or directory) to
// indicate whether the particular inode in the inode table is in use
#define INODE_BITMAP_SIZE ((MAX_FILES+7)/8) //why 7? doing  ceilling!
#define INODE_BITMAP_SECTORS ((INODE_BITMAP_SIZE+SECTOR_SIZE-1)/SECTOR_SIZE) //ceilling!

// 3. the sector bitmap (one or more sectors), which indicates whether
// the particular sector in the disk is currently in use
#define SECTOR_BITMAP_START_SECTOR (INODE_BITMAP_START_SECTOR+INODE_BITMAP_SECTORS)

// the total number of bytes and sectors needed for the data block
// bitmap (we call it the sector bitmap); we use one bit for each
// sector of the disk to indicate whether the sector is in use or not
#define SECTOR_BITMAP_SIZE ((TOTAL_SECTORS+7)/8)
#define SECTOR_BITMAP_SECTORS ((SECTOR_BITMAP_SIZE+SECTOR_SIZE-1)/SECTOR_SIZE)
char all_bitmap[INODE_BITMAP_SECTORS+INODE_BITMAP_SECTORS][512];
// 4. the inode table (one or more sectors), which contains the inodes
// stored consecutively
#define INODE_TABLE_START_SECTOR (SECTOR_BITMAP_START_SECTOR+SECTOR_BITMAP_SECTORS)

// an inode is used to represent each file or directory; the data
// structure supposedly contains all necessary information about the
// corresponding file or directory
typedef struct _inode {
  int size; // the size of the file or number of directory entries
  int type; // 0 means regular file; 1 means directory
  int data[MAX_SECTORS_PER_FILE]; // indices to sectors containing data blocks
} inode_t;

// the inode structures are stored consecutively and yet they don't
// straddle accross the sector boundaries; that is, there may be
// fragmentation towards the end of each sector used by the inode
// table; each entry of the inode table is an inode structure; there
// are as many entries in the table as the number of files allowed in
// the system; the inode bitmap (#2) indicates whether the entries are
// current in use or not
#define INODES_PER_SECTOR (SECTOR_SIZE/sizeof(inode_t))
#define INODE_TABLE_SECTORS ((MAX_FILES+INODES_PER_SECTOR-1)/INODES_PER_SECTOR)

// 5. the data blocks; all the rest sectors are reserved for data
// blocks for the content of files and directories
#define DATABLOCK_START_SECTOR (INODE_TABLE_START_SECTOR+INODE_TABLE_SECTORS)

// other file related definitions

// max length of a path is 256 bytes (including the ending null)
#define MAX_PATH 256

// max length of a filename is 16 bytes (including the ending null)
#define MAX_NAME 16

// max number of open files is 256
#define MAX_OPEN_FILES 256

// each directory entry represents a file/directory in the parent
// directory, and consists of a file/directory name (less than 16
// bytes) and an integer inode number
typedef struct _dirent {
  char fname[MAX_NAME]; // name of the file
  int inode; // inode of the file
} dirent_t;

// the number of directory entries that can be contained in a sector
#define DIRENTS_PER_SECTOR (SECTOR_SIZE/sizeof(dirent_t))

// global errno value here
int osErrno;

// the name of the disk backstore file (with which the file system is booted)
static char bs_filename[1024];


/* the following functions are internal helper functions */

// check magic number in the superblock; return 1 if OK, and 0 if not
static int check_magic()
{
  char buf[SECTOR_SIZE];
  if(Disk_Read(SUPERBLOCK_START_SECTOR, buf) < 0)
    return 0;
  if(*(int*)buf == OS_MAGIC) return 1;
  else return 0;
}

// initialize a bitmap with 'num' sectors starting from 'start'
// sector; all bits should be set to zero except that the first
// 'nbits' number of bits are set to one
static void bitmap_init(int start, int num, int nbits)
{
  /* YOUR CODE */
  int pos;
  int set_bit;
  int i, cnt=0, b;
  char _bitmap[SECTOR_SIZE];

  for(i=start;i<start+num;i++)
  {
	  //each sector
	  memset(_bitmap, 0, SECTOR_SIZE);
	  for(pos=0;pos<512;pos++)
	  {
		//each sector has 512 bytes
		//set 512 bytes
		//each byte has 8 bits
		
		for(b=0;b<8;b++)
		{
			if(cnt<=nbits)
        	                set_bit = 128 >> b;
	                else
                	        set_bit = 0;
                	cnt++;
			_bitmap[pos] = _bitmap[pos] | set_bit;

		}
		
	  }

	  Disk_Write(i, _bitmap);

	  
  }
  
}

// set the first unused bit from a bitmap of 'nbits' bits (flip the
// first zero appeared in the bitmap to one) and return its location;
// return -1 if the bitmap is already full (no more zeros)
static int bitmap_first_unused(int start, int num, int nbits)
{
  /* YOUR CODE */
  
  int pos = 0;
  int j=0;
  unsigned int set_bit;
  char t;
  int k=0;
  int i;
  char _bitmap[SECTOR_SIZE];
  for(i=start;i<start+num;i++)
  {
	  Disk_Read(i, _bitmap);
	  for(j=0;j<SECTOR_SIZE;j++)
	  {
		for(k=0;k<8;k++)
		{
			set_bit = 128 >> k;
			t = _bitmap[j] & set_bit;
			if(t==0)
			{
				_bitmap[j] = _bitmap[j] | set_bit;
				Disk_Write(i, _bitmap);        
				return pos;
			}
			
			pos++;
			
		}
	  }
	  
  }
  
  return -1;
}


// reset the i-th bit of a bitmap with 'num' sectors starting from
// 'start' sector; return 0 if successful, -1 otherwise
static int bitmap_reset(int start, int num, int ibit)
{
  /* YOUR CODE */
  
  int pos = 0;
  int j=0;
  unsigned int set_bit;
  int k=0;
  int i;
  char _bitmap[SECTOR_SIZE];
  for(i=start;i<num+start;i++)
  {
	  Disk_Read(i, _bitmap);
	  for(j=0;j<SECTOR_SIZE;j++)
	  {
		for(k=0;k<8;k++)
		{
			set_bit = 128 >> k;
			//t = _bitmap[j] & set_bit;
			if(pos==ibit)
			{
				_bitmap[j] = _bitmap[j] & (~set_bit);
				Disk_Write(i, _bitmap);
				return 0;
			}
			
			pos++;
			
		}
	  }
	  
  }
  
  return -1;
}

// return 1 if the file name is illegal; otherwise, return 0; legal
// characters for a file name include letters (case sensitive),
// numbers, dots, dashes, and underscores; and a legal file name
// should not be more than MAX_NAME-1 in length
static int illegal_filename(char* name)
{
  /* YOUR CODE */
int i,count=0;
    //printf("%d",strlen(name));
    if(strlen(name)<=MAX_NAME-1)
    {
    for(i=0;i<strlen(name);i++)
    {
        if((name[i]>=65 && name[i]<=90) ||(name[i]>=97 && name[i]<=122)||(name[i]>=48 && name[i]<=57) || name[i] == 45 || name[i] == 46 || name[i] == 95)
            count=count;
           // printf("%c\n",name[i]);
        else
            count++;
    }
    if(count == 0)
        return 0;
    else
        return 1;
    }
    else
        return 1;
        //printf("Illegal Filename. More than Maximum Name Size.");
  //return 0; 
}

// return the child inode of the given file name 'fname' from the
// parent inode; the parent inode is currently stored in the segment
// of inode table in the cache (we cache only one disk sector for
// this); once found, both cached_inode_sector and cached_inode_buffer
// may be updated to point to the segment of inode table containing
// the child inode; the function returns -1 if no such file is found;
// it returns -2 is something else is wrong (such as parent is not
// directory, or there's read error, etc.)
static int find_child_inode(int parent_inode, char* fname,
			    int *cached_inode_sector, char* cached_inode_buffer)
{
  int cached_start_entry = ((*cached_inode_sector)-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  int offset = parent_inode-cached_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* parent = (inode_t*)(cached_inode_buffer+offset*sizeof(inode_t));
  dprintf("... load parent inode: %d (size=%d, type=%d)\n",
	 parent_inode, parent->size, parent->type);
  if(parent->type != 1) {
    dprintf("... parent not a directory\n");
    return -2;
  }

  int nentries = parent->size; // remaining number of directory entries 
  int idx = 0;
  while(nentries > 0) {
    char buf[SECTOR_SIZE]; // cached content of directory entries
    if(Disk_Read(parent->data[idx], buf) < 0) return -2;
    for(int i=0; i<DIRENTS_PER_SECTOR; i++) {
      if(i>nentries) break;
      if(!strcmp(((dirent_t*)buf)[i].fname, fname)) {
	// found the file/directory; update inode cache
	int child_inode = ((dirent_t*)buf)[i].inode;
	dprintf("... found child_inode=%d\n", child_inode);
	int sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
	if(sector != (*cached_inode_sector)) {
	  *cached_inode_sector = sector;
	  if(Disk_Read(sector, cached_inode_buffer) < 0) return -2;
	  dprintf("... load inode table for child\n");
	}
	return child_inode;
      }
    }
    idx++; nentries -= DIRENTS_PER_SECTOR;
  }
  dprintf("... could not find child inode\n");
  return -1; // not found
}

// follow the absolute path; if successful, return the inode of the
// parent directory immediately before the last file/directory in the
// path; for example, for '/a/b/c/d.txt', the parent is '/a/b/c' and
// the child is 'd.txt'; the child's inode is returned through the
// parameter 'last_inode' and its file name is returned through the
// parameter 'last_fname' (both are references); it's possible that
// the last file/directory is not in its parent directory, in which
// case, 'last_inode' points to -1; if the function returns -1, it
// means that we cannot follow the path
static int follow_path(char* path, int* last_inode, char* last_fname)
{
  if(!path) {
    dprintf("... invalid path\n");
    return -1;
  }
  if(path[0] != '/') {
    dprintf("... '%s' not absolute path\n", path);
    return -1;
  }
  
  // make a copy of the path (skip leading '/'); this is necessary
  // since the path is going to be modified by strsep()
  char pathstore[MAX_PATH]; 
  strncpy(pathstore, path+1, MAX_PATH-1);
  pathstore[MAX_PATH-1] = '\0'; // for safety
  char* lpath = pathstore;
  
  int parent_inode = -1, child_inode = 0; // start from root
  // cache the disk sector containing the root inode
  int cached_sector = INODE_TABLE_START_SECTOR;
  char cached_buffer[SECTOR_SIZE];
  if(Disk_Read(cached_sector, cached_buffer) < 0) return -1;
  dprintf("... load inode table for root from disk sector %d\n", cached_sector);
  
  // for each file/directory name separated by '/'
  char* token;
  while((token = strsep(&lpath, "/")) != NULL) {
    dprintf("... process token: '%s'\n", token);
    if(*token == '\0') continue; // multiple '/' ignored
    if(illegal_filename(token)) {
      dprintf("... illegal file name: '%s'\n", token);
      return -1; 
    }
    if(child_inode < 0) {
      // regardless whether child_inode was not found previously, or
      // there was issues related to the parent (say, not a
      // directory), or there was a read error, we abort
      dprintf("... parent inode can't be established\n");
      return -1;
    }
    parent_inode = child_inode;
    child_inode = find_child_inode(parent_inode, token,
				   &cached_sector, cached_buffer);
    if(last_fname) strcpy(last_fname, token);
  }
  if(child_inode < -1) return -1; // if there was error, abort
  else {
    // there was no error, several possibilities:
    // 1) '/': parent = -1, child = 0
    // 2) '/valid-dirs.../last-valid-dir/not-found': parent=last-valid-dir, child=-1
    // 3) '/valid-dirs.../last-valid-dir/found: parent=last-valid-dir, child=found
    // in the first case, we set parent=child=0 as special case
    if(parent_inode==-1 && child_inode==0) parent_inode = 0;
    dprintf("... found parent_inode=%d, child_inode=%d\n", parent_inode, child_inode);
    *last_inode = child_inode;
    return parent_inode;
  }
}

// given a path, finds the inode representing the file/directory at that path
// returns 0 on success and assigns the found inode_t to `inode`
int get_inode_from_path(char* path, inode_t** inode)
{
  char last_name[MAX_NAME];
  int child_inode;
  if (follow_path(path, &child_inode, last_name) >= 0)
  {
    assert(child_inode >= 0);

    int inode_sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
    char cached_buffer[SECTOR_SIZE];
    if(Disk_Read(inode_sector, cached_buffer) < 0) return -1;

    int cached_start_entry = ((inode_sector)-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
    int offset = child_inode-cached_start_entry;
    
    assert(0 <= offset && offset < INODES_PER_SECTOR);
    *inode = (inode_t*)(cached_buffer+offset*sizeof(inode_t));

    return 0;
  }
  return -1;
}

// add a new file or directory (determined by 'type') of given name
// 'file' under parent directory represented by 'parent_inode'
int add_inode(int type, int parent_inode, char* file)
{
  // get a new inode for child
  int child_inode = bitmap_first_unused(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, INODE_BITMAP_SIZE);
  if(child_inode < 0) {
    dprintf("... error: inode table is full\n");
    return -1; 
  }
  dprintf("... new child inode %d\n", child_inode);

  // load the disk sector containing the child inode
  int inode_sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
  char inode_buffer[SECTOR_SIZE];
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for child inode from disk sector %d\n", inode_sector);

  // get the child inode
  int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  int offset = child_inode-inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* child = (inode_t*)(inode_buffer+offset*sizeof(inode_t));

  // update the new child inode and write to disk
  memset(child, 0, sizeof(inode_t));
  child->type = type;
  if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... update child inode %d (size=%d, type=%d), update disk sector %d\n",
	 child_inode, child->size, child->type, inode_sector);

  // get the disk sector containing the parent inode
  inode_sector = INODE_TABLE_START_SECTOR+parent_inode/INODES_PER_SECTOR;
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for parent inode %d from disk sector %d\n",
	 parent_inode, inode_sector);

  // get the parent inode
  inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  offset = parent_inode-inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* parent = (inode_t*)(inode_buffer+offset*sizeof(inode_t));
  dprintf("... get parent inode %d (size=%d, type=%d)\n",
	 parent_inode, parent->size, parent->type);

  // get the dirent sector
  if(parent->type != 1) {
    dprintf("... error: parent inode is not directory\n");
    return -2; // parent not directory
  }
  int group = parent->size/DIRENTS_PER_SECTOR;
  char dirent_buffer[SECTOR_SIZE];
  if(group*DIRENTS_PER_SECTOR == parent->size) {
    // new disk sector is needed
    int newsec = bitmap_first_unused(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE);
    if(newsec < 0) {
      dprintf("... error: disk is full\n");
      return -1;
    }
    parent->data[group] = newsec;
    memset(dirent_buffer, 0, SECTOR_SIZE);
    dprintf("... new disk sector %d for dirent group %d\n", newsec, group);
  } else {
    if(Disk_Read(parent->data[group], dirent_buffer) < 0)
      return -1;
    dprintf("... load disk sector %d for dirent group %d\n", parent->data[group], group);
  }

  // add the dirent and write to disk
  int start_entry = group*DIRENTS_PER_SECTOR;
  offset = parent->size-start_entry;
  dirent_t* dirent = (dirent_t*)(dirent_buffer+offset*sizeof(dirent_t));
  strncpy(dirent->fname, file, MAX_NAME);
  dirent->inode = child_inode;
  if(Disk_Write(parent->data[group], dirent_buffer) < 0) return -1;
  dprintf("... append dirent %d (name='%s', inode=%d) to group %d, update disk sector %d\n",
	  parent->size, dirent->fname, dirent->inode, group, parent->data[group]);

  // update parent inode and write to disk
  parent->size++;
  if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... update parent inode on disk sector %d\n", inode_sector);
  
  return 0;
}

// used by both File_Create() and Dir_Create(); type=0 is file, type=1
// is directory
int create_file_or_directory(int type, char* pathname)
{
  int child_inode;
  char last_fname[MAX_NAME];
  int parent_inode = follow_path(pathname, &child_inode, last_fname);
  if(parent_inode >= 0) {
    if(child_inode >= 0) {
      dprintf("... file/directory '%s' already exists, failed to create\n", pathname);
      osErrno = E_CREATE;
      return -1;
    } else {
      if(add_inode(type, parent_inode, last_fname) >= 0) {
	dprintf("... successfully created file/directory: '%s'\n", pathname);
	return 0;
      } else {
	dprintf("... error: something wrong with adding child inode\n");
	osErrno = E_CREATE;
	return -1;
      }
    }
  } else {
    dprintf("... error: something wrong with the file/path: '%s'\n", pathname);
    osErrno = E_CREATE;
    return -1;
  }
}

// remove the child from parent; the function is called by both
// File_Unlink() and Dir_Unlink(); the function returns 0 if success,
// -1 if general error, -2 if directory not empty, -3 if wrong type
int remove_inode(int type, int parent_inode, int child_inode)
{
  /* YOUR CODE */

  // first, read the sector containing the child inode

  dprintf("... removing inode %d from parent %d\n", child_inode, parent_inode);

  int inode_sector = INODE_TABLE_START_SECTOR + child_inode / INODES_PER_SECTOR;
  char inode_buffer[SECTOR_SIZE];
  if(Disk_Read(inode_sector, inode_buffer) < 0) { osErrno = E_GENERAL; return -1; }
  dprintf("... load inode table for inode from disk sector %d\n", inode_sector);

  // reusing code from the sample code here
  int cached_start_entry = ((inode_sector) - INODE_TABLE_START_SECTOR) *
    INODES_PER_SECTOR;
  int offset = child_inode - cached_start_entry;

  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* child_inode_t = (inode_t*)(inode_buffer + (offset * sizeof(inode_t)));

  // ensure type consistency
  if (type != child_inode_t->type)
  {
    dprintf("... ERROR: type given to remove_inode does not match found inode \
      type\n");
    // return -3 error code for wrong type
    return -3;
  }

  // if directory, ensure directory is non-empty
  if (child_inode_t->type == 1 && child_inode_t->size > 0)
  {
    dprintf("... ERROR: remove_inode called on inode for non-empty directory\n");
    return -2;
  }

  // if we got here, neither of the above error conditions are true, so delete
  // the inode and update the disk sector
  dprintf("... deleting inode %d and writing back to disk\n", child_inode);
  memset(child_inode_t, 0, sizeof(inode_t));
  if (Disk_Write(inode_sector, inode_buffer) < 0) return -1;

  // reset bit of child inode in bitmap
  if (bitmap_reset(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, child_inode) < 0) {
    dprintf("... ERROR: unable to reset inode bit in bitmap at index %d\n", child_inode);
    return -1;
  }

  // next, we need to update the parent inode to reflect the child has been deleted

  // calculate the parent's sector
  inode_sector = INODE_TABLE_START_SECTOR + parent_inode/ INODES_PER_SECTOR;
  if(Disk_Read(inode_sector, inode_buffer) < 0) { osErrno = E_GENERAL; return -1; }
  dprintf("... load inode table for inode from disk sector %d\n", inode_sector);

  // reusing code from the sample code, again, to get parent inode
  cached_start_entry = (inode_sector - INODE_TABLE_START_SECTOR) * INODES_PER_SECTOR;
  offset = parent_inode - cached_start_entry;

  assert(0 <= offset && offset < INODES_PER_SECTOR);

  inode_t* parent = (inode_t*)(inode_buffer + offset * sizeof(inode_t));
  dprintf("... get parent inode %d (size=%d, type=%d)\n",
	  parent_inode, parent->size, parent->type);

  // reusing more sample code here, modified to fit this method
  int deleted = 0;
  int total_sectors = (parent->size + DIRENTS_PER_SECTOR - 1) / DIRENTS_PER_SECTOR;
  // remainder here: if value is > 0 then there is a partial dirent sector
  int remain_dirents = (parent->size % DIRENTS_PER_SECTOR);
  int num_dirents = 0;
    
  char buf[SECTOR_SIZE]; // cached content of directory entries
  dirent_t *dirent, *final;
  int idx_sector, idx_dirent;

  for (idx_sector = 0; idx_sector < total_sectors; idx_sector++)
  { 
    if (Disk_Read(parent->data[idx_sector], buf) < 0)
      return -2;

    for (idx_dirent = 0; idx_dirent < DIRENTS_PER_SECTOR; idx_dirent++)
    {
      num_dirents++;

      if (deleted == 0 && num_dirents <= parent->size)
      {
        dirent = (dirent_t*)(buf + idx_dirent * sizeof(dirent_t));
        // the following cases are necessary to handle edge cases when a file is deleted in a filesystem
        // also containing a directory. in such a case, naively deleting the child dirent and returning
        // can result in an inconsistent state where inodes are repeated and files cannot be read.
        if (dirent->inode == child_inode)
        {
          // if this is true, it means the child dirent and final dirent are in the same sector
          if (idx_sector == total_sectors - 1)
          {
            final = (dirent_t*)(buf + ((remain_dirents - 1) * sizeof(dirent_t)));
            memcpy(dirent, final, sizeof(dirent_t));
            memset(final, 0, sizeof(dirent_t));

            // write to disk the updated sector
            if (Disk_Write(parent->data[idx_sector], buf) < 0)
            {
              dprintf("... error writing to parent data\n");
              return -1;
            }
            dprintf("... successfully deleted dirent for child inode %d\n", child_inode);
            deleted = 1;
          }
          else
          {
            // otherwise, the child inode exists but is not in the final sector
            char final_buf[SECTOR_SIZE];

            if (Disk_Read(parent->data[total_sectors - 1], final_buf) < 0)
            {
              dprintf("... error reading final sector of parent %d\n", parent_inode);
              return -1;
            }

            final = (dirent_t*)(final_buf + ((remain_dirents - 1) * sizeof(dirent_t)));
            memcpy(dirent, final, sizeof(dirent_t));
            memset(final, 0, sizeof(dirent_t));

            if (Disk_Write(parent->data[idx_sector], buf) < 0)
            {
              dprintf("... error writing to parent data\n");
              return -1;
            }
            if (Disk_Write(parent->data[total_sectors - 1], final_buf) < 0)
            {
              dprintf("... error writing final sector to parent data\n");
              return -1;
            }
            dprintf("... successfully deleted dirent for child inode %d\n", child_inode);
            deleted = 1;
          }
        }
      }
      else if (deleted == 0 && dirent)
      {
        // if we get here, it means the child is the last dirent in the parent
        if (dirent->inode ==  child_inode)
        {
            memset(dirent, 0, sizeof(dirent_t));
            if (Disk_Write(parent->data[idx_sector], buf) < 0)
            {
              dprintf("... error writing to final dirent of parent\n");
              return -1;
            }
            deleted = 1;
        }
        else
        {
          dprintf("... error: could not find child dirent in parent\n");
          return -1;
        }
      }
    }
  }

  parent->size--;

  if (Disk_Write(inode_sector, inode_buffer) < 0)
  {
    dprintf("... error writing updated inode table to disk\n");
    return 1;
  }
  else
  {
    dprintf("... inode %d successfully unlinked\n", child_inode);
    return 0;
  }
}

// representing an open file
typedef struct _open_file {
  int inode; // pointing to the inode of the file (0 means entry not used)
  int size;  // file size cached here for convenience
  int pos;   // read/write position
} open_file_t;
static open_file_t open_files[MAX_OPEN_FILES];

// return true if the file pointed to by inode has already been open
int is_file_open(int inode)
{
  for(int i=0; i<MAX_OPEN_FILES; i++) {
    if(open_files[i].inode == inode)
      return 1;
  }
  return 0;
}

// return a new file descriptor not used; -1 if full
int new_file_fd()
{
  for(int i=0; i<MAX_OPEN_FILES; i++) {
    if(open_files[i].inode <= 0)
      return i;
  }
  return -1;
}

/* end of internal helper functions, start of API functions */

int FS_Boot(char* backstore_fname)
{
  dprintf("FS_Boot('%s'):\n", backstore_fname);
  // initialize a new disk (this is a simulated disk)
  if(Disk_Init() < 0) {
    dprintf("... disk init failed\n");
    osErrno = E_GENERAL;
    return -1;
  }
  dprintf("... disk initialized\n");
  
  // we should copy the filename down; if not, the user may change the
  // content pointed to by 'backstore_fname' after calling this function
  strncpy(bs_filename, backstore_fname, 1024);
  bs_filename[1023] = '\0'; // for safety
  
  // we first try to load disk from this file
  if(Disk_Load(bs_filename) < 0) {
    dprintf("... load disk from file '%s' failed\n", bs_filename);

    // if we can't open the file; it means the file does not exist, we
    // need to create a new file system on disk
    if(diskErrno == E_OPENING_FILE) {
      dprintf("... couldn't open file, create new file system\n");

      // format superblock
      char buf[SECTOR_SIZE];
      memset(buf, 0, SECTOR_SIZE);
      *(int*)buf = OS_MAGIC;
      if(Disk_Write(SUPERBLOCK_START_SECTOR, buf) < 0) {
	dprintf("... failed to format superblock\n");
	osErrno = E_GENERAL;
	return -1;
      }
      dprintf("... formatted superblock (sector %d)\n", SUPERBLOCK_START_SECTOR);

      // format inode bitmap (reserve the first inode to root)
      bitmap_init(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, 1);
      dprintf("... formatted inode bitmap (start=%d, num=%d)\n",
	     (int)INODE_BITMAP_START_SECTOR, (int)INODE_BITMAP_SECTORS);
      
      // format sector bitmap (reserve the first few sectors to
      // superblock, inode bitmap, sector bitmap, and inode table)
      bitmap_init(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS,
		  DATABLOCK_START_SECTOR);
      dprintf("... formatted sector bitmap (start=%d, num=%d)\n",
	     (int)SECTOR_BITMAP_START_SECTOR, (int)SECTOR_BITMAP_SECTORS);
      
      // format inode tables
      for(int i=0; i<INODE_TABLE_SECTORS; i++) {
	memset(buf, 0, SECTOR_SIZE);
	if(i==0) {
	  // the first inode table entry is the root directory
	  ((inode_t*)buf)->size = 0;
	  ((inode_t*)buf)->type = 1;
	}
	if(Disk_Write(INODE_TABLE_START_SECTOR+i, buf) < 0) {
	  dprintf("... failed to format inode table\n");
	  osErrno = E_GENERAL;
	  return -1;
	}
      }
      dprintf("... formatted inode table (start=%d, num=%d)\n",
	     (int)INODE_TABLE_START_SECTOR, (int)INODE_TABLE_SECTORS);
      
      // we need to synchronize the disk to the backstore file (so
      // that we don't lose the formatted disk)
      if(Disk_Save(bs_filename) < 0) {
	// if can't write to file, something's wrong with the backstore
	dprintf("... failed to save disk to file '%s'\n", bs_filename);
	osErrno = E_GENERAL;
	return -1;
      } else {
	// everything's good now, boot is successful
	dprintf("... successfully formatted disk, boot successful\n");
	memset(open_files, 0, MAX_OPEN_FILES*sizeof(open_file_t));
	return 0;
      }
    } else {
      // something wrong loading the file: invalid param or error reading
      dprintf("... couldn't read file '%s', boot failed\n", bs_filename);
      osErrno = E_GENERAL; 
      return -1;
    }
  } else {
    dprintf("... load disk from file '%s' successful\n", bs_filename);
    
    // we successfully loaded the disk, we need to do two more checks,
    // first the file size must be exactly the size as expected (thiis
    // supposedly should be folded in Disk_Load(); and it's not)
    int sz = 0;
    FILE* f = fopen(bs_filename, "r");
    if(f) {
      fseek(f, 0, SEEK_END);
      sz = ftell(f);
      fclose(f);
    }
    if(sz != SECTOR_SIZE*TOTAL_SECTORS) {
      dprintf("... check size of file '%s' failed\n", bs_filename);
      osErrno = E_GENERAL;
      return -1;
    }
    dprintf("... check size of file '%s' successful\n", bs_filename);
    
    // check magic
    if(check_magic()) {
      // everything's good by now, boot is successful
      dprintf("... check magic successful\n");
      memset(open_files, 0, MAX_OPEN_FILES*sizeof(open_file_t));
      return 0;
    } else {      
      // mismatched magic number
      dprintf("... check magic failed, boot failed\n");
      osErrno = E_GENERAL;
      return -1;
    }
  }
}

int FS_Sync()
{
  if(Disk_Save(bs_filename) < 0) {
    // if can't write to file, something's wrong with the backstore
    dprintf("FS_Sync():\n... failed to save disk to file '%s'\n", bs_filename);
    osErrno = E_GENERAL;
    return -1;
  } else {
    // everything's good now, sync is successful
    dprintf("FS_Sync():\n... successfully saved disk to file '%s'\n", bs_filename);
    return 0;
  }
}

int File_Create(char* file)
{
  dprintf("File_Create('%s'):\n", file);
  return create_file_or_directory(0, file);
}

int File_Unlink(char* file)
{
  /* YOUR CODE */

  dprintf("File_Unlink('%s'):\n", file);
  int child_inode;
  char child_fname[MAX_NAME];
  int parent_inode = follow_path(file, &child_inode, child_fname);

  return remove_inode(0, parent_inode, child_inode);
}

int File_Open(char* file)
{
  dprintf("File_Open('%s'):\n", file);
  int fd = new_file_fd();
  if(fd < 0) {
    dprintf("... max open files reached\n");
    osErrno = E_TOO_MANY_OPEN_FILES;
    return -1;
  }

  int child_inode;
  follow_path(file, &child_inode, NULL);
  if(child_inode >= 0) { // child is the one
    // load the disk sector containing the inode
    int inode_sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
    char inode_buffer[SECTOR_SIZE];
    if(Disk_Read(inode_sector, inode_buffer) < 0) { osErrno = E_GENERAL; return -1; }
    dprintf("... load inode table for inode from disk sector %d\n", inode_sector);

    // get the inode
    int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
    int offset = child_inode-inode_start_entry;
    assert(0 <= offset && offset < INODES_PER_SECTOR);
    inode_t* child = (inode_t*)(inode_buffer+offset*sizeof(inode_t));
    dprintf("... inode %d (size=%d, type=%d)\n",
	    child_inode, child->size, child->type);

    if(child->type != 0) {
      dprintf("... error: '%s' is not a file\n", file);
      osErrno = E_GENERAL;
      return -1;
    }

    // initialize open file entry and return its index
    open_files[fd].inode = child_inode;
    open_files[fd].size = child->size;
    open_files[fd].pos = 0;
    return fd;
  } else {
    dprintf("... file '%s' is not found\n", file);
    osErrno = E_NO_SUCH_FILE;
    return -1;
  }  
}

int File_Read(int fd, void* buffer, int size)
{
  /* YOUR CODE */

	int file_inode = open_files[fd].inode; // file_inode will have the inode number for the file to be read.
                                              //Here, open_files is a structure containing the value of inode with variable name inode.
	if(!file_inode) // If the file is not open (i.e open_files[fd].inode returns 0), return -1, and set osErrno to E_BAD_FD.
  {  
		osErrno = E_BAD_FD;  
		return -1;
	}

	// load the disk sector containing the child inode
     
	int inode_sector = INODE_TABLE_START_SECTOR+file_inode/INODES_PER_SECTOR; //inode_sector will have the sector number that contains file_inode which is our required file inode.
	char inode_buffer[SECTOR_SIZE]; 

        // from the line below, after Dish_read "inode_buffer" will contain the inode_sector's content
	if(Disk_Read(inode_sector, inode_buffer) < 0) return -1; 

	dprintf("... load inode table for child inode from disk sector %d\n", inode_sector);
	
	
	// get the child inode  
	int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR; // inode_start_entry will have inode number that will be in the start of the inode_sector
	int offset = file_inode-inode_start_entry; // now we have the offset to go from the inode_start_entry to reach to our desired inode.
	assert(0 <= offset && offset < INODES_PER_SECTOR); // offset can be 0 to INODE_PER_SECTOR. otherwise assert function will give an error message and abort program execution
	inode_t* file = (inode_t*)(inode_buffer+offset*sizeof(inode_t)); // Here, inode_buffer will provide starting address of inode_buffer[]. but we need only our 
                                                                         // desired file_inode content.so we are adding offset adress with starting address to get there.
                                                                         // Now, file will point to the file_inode.

	// Necessary variables needed for file_read
        int i,j,count=0;
        char* charBuffer = (char*) buffer;
        char temp_buffer[SECTOR_SIZE];
	int s_pos = open_files[fd].pos;


	// define start variables to read starting from file pointer
	int start_sector = open_files[fd].pos / SECTOR_SIZE; // start_sector will have the sector number containing pos. pos, we know from the open_file structure that it read/write position
	int startbyte = open_files[fd].pos % SECTOR_SIZE; // initialization of startbyte where it will have the specific byte number of that sector indicated by pos
 


	// while loop will read the content of the data sectors allocated to the file we want to read sequentially
         i=start_sector;
	
         while(i < MAX_SECTORS_PER_FILE)
         {
		if (file->data[i]) // "file->data[i]" from the specific inode will provide the sector numbers containg the data for the file, one by one.
                      {
			Disk_Read(file->data[i], temp_buffer); // Disk_Read will read the sector data which will be read into a temporary buffer named temp_buffer. 
			dprintf("\n From sector %d we have Buffer contents: %s", file->data[i], temp_buffer);
                        j=startbyte;  //for first sector it may start from any other postition rather than 0.
			while(j < SECTOR_SIZE)
                        {	
				if ((count+s_pos < file->size) && (count < size)){	// while still inside of the file & reading into size of buffer
					charBuffer[count++] = temp_buffer[j]; // content of temporary buffer will be copied to charbuffer. at first iteration it will be started from pos
                                                                // later on it will start copying from first position of temp_buffer.
				} 
                        j++;
			}
	
			startbyte = 0; // after the first sector, the later sectors will be read fully , so startb (start byte) will be 0 here for reading buf
                                    // so reset
		}
        i++;
	}
	
	open_files[fd].pos += count; // value of the pos variable will be updated with new one

	return count; // by returning count, the function is providing the number of bytes actually read which can be less than or equal to size.
}
 

int File_Write(int fd, void* buffer, int size)
{
  /* YOUR CODE */
  int file_inode = open_files[fd].inode; // file_inode will have the inode number for the file where we want to write the content of buffer.
                                              //Here, open_files is a structure containing the value of inode with variable name inode.
	if(!file_inode) {                    // If the file is not open (i.e open_files[fd].inode returns 0), it will show the error message and return -1, and set osErrno to E_BAD_FD.
		osErrno = E_BAD_FD;
		printf("File was not opened\n");
		return -1;
	}

	// load the disk sector containing the child inode
	int inode_sector = INODE_TABLE_START_SECTOR+file_inode/INODES_PER_SECTOR; //inode_sector will have the sector number that contains file_inode which is our required file inode.
	char inode_buffer[SECTOR_SIZE];
        
        // from the line below, after Disk_read "inode_buffer" will contain the inode_sector's content 
	if(Disk_Read(inode_sector, inode_buffer) < 0) return -1; 
	dprintf("... load inode table for child inode from disk sector %d\n", inode_sector);
	
	
	// get the child inode
	int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR; // inode_start_entry will have inode number that will be in the start of the inode_sector
	int offset = file_inode-inode_start_entry; // now we have the offset to go from the inode_start_entry to reach to our desired inode.
	assert(0 <= offset && offset < INODES_PER_SECTOR); // offset can be 0 to INODE_PER_SECTOR. otherwise assert function will give an error message and abort program execution
	inode_t* file = (inode_t*)(inode_buffer+offset*sizeof(inode_t)); // Here, inode_buffer will provide starting address of inode_buffer[]. but we need only our 
                                                                         // desired file_inode content.so we are adding offset adress with starting address to get there.
                                                                         // Now, file will point to the file_inode.
        


	if((file->size + size) > MAX_FILE_SIZE){  // "file->size" will provide the current size of the file where we want to write 
                                                  // It will be added with "size" which is the size of the data we want to write to file from buffer.
                                                 // Thus if the file exceeds the maximum file size, it would return -1 and set osErrno to E_FILE_TOO_BIG showing an error message.
		osErrno = E_FILE_TOO_BIG;
		printf("File was too big\n");
		return -1;
	}
        
        // Necessary variables needed for file_write
	int i, j,count=0;
	char* charBuffer = (char*) buffer;
	int t_size = size;
        char temp_buffer[SECTOR_SIZE];

        // We may have to write data to a sector from starting byte or pos can be in any other byte

	int start_sector = open_files[fd].pos / SECTOR_SIZE; // start_sector will have the sector number containing pos. pos, we know from the open_file structure that it is read/write position
	int startbyte = open_files[fd].pos % SECTOR_SIZE; // initialization of startb where it will have the specific byte number of that sector indicated by pos 

	// while loop will write the content of buffer to the data sectors allocated to the file sequentially. 
        // first have to check if we will start writing to an already existing sector or have to allocate e new one before writing. 
        i=start_sector;
        while((i < MAX_SECTORS_PER_FILE) && (count<size))
        {       
		
               //if we are writing in an already allocated sectors
		if (file->data[i]) // "file->data[i]" from the specific inode will provide the sector number where to write
                {  
			Disk_Read(file->data[i], temp_buffer); // Disk_Read will read the previously existing sector data which will be saved into buf.
                        j=startbyte;
			while(j < SECTOR_SIZE)
                        {
				// write data to buffer
				if(t_size-- > 0) 
                                    temp_buffer[j] = charBuffer[count++]; // buffer[] has the content to write in the sector. It will be copied to temp_buffer[] from the startbyte index 
                                                           // that is from the byte we want to write the buffer[] value.
                        j++;

			}
			startbyte = 0; // after the first sector, in the later sectors buffer[] will written from the starting byte , so startbyte will be 0 here for writing
                                      // so reset

			dprintf("In sector %d we are writing: %s\n", file->data[i], temp_buffer); // print the contents of particular sectors

			Disk_Write(file->data[i], temp_buffer); // Now, Disk_Write will write the temp_buff content into the sector specified by "file->data[i]" 
		}
		else // if "file->data[i]" won't provide any sector index that means we have to write into a new file sector which is not yet allocated to that file
                    //thus have to allocate new sector 
                {   
			
			int newsector = bitmap_first_unused(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE);
			if(newsector == -1) //if there is no new sector available, the write cannot be completed due to a lack of space on disk. It will set osErrno to E_NO_SPACE.
                        {
				osErrno = E_NO_SPACE; 
				return -1;
			}
			file->data[i] = newsector; // "file->data[i]" value will be updated with new sector value newsector 

			char temp_buffer2[SECTOR_SIZE];

			memset(temp_buffer2, 0, SECTOR_SIZE); // temp_buffer will be intialized with 0 upto SECTOR_SIZE (512B here)
                        
                        j=0;

			while(j < SECTOR_SIZE)
                        {
				// write to temp_buffer2
				if(t_size-- > 0)
                                   temp_buffer2[j] = charBuffer[count++]; // buffer[] has the content to write in the sector. It will be copied to temp_buffer2[] from the buffer[]
                         j++;
			}

			//save changes
			dprintf("\nIn new sector %d we are writing: %s\n", file->data[i], temp_buffer2); 
			Disk_Write(file->data[i], temp_buffer2);// Now, Disk_Write will write the temp_buffer2 content into the sector specified by "file->data[i]" 
		}
           i++;
	}

	//save changes 
	open_files[fd].pos += size; // new position value for write will be set to pos
	file->size += size; // inode content for the particular will be updated incrementing the file size by adding the new buffer size
	open_files[fd].size += size; // open_file structure content will be changed as well by incrementing the size of the file with new adding new buffer size

	Disk_Write(inode_sector, inode_buffer); // inode_sector that means the sector of the file inode will be updated with new inode_buffer.

	return size;
}
int File_Seek(int fd, int offset)
{
  /* YOUR CODE */
	int file_inode = open_files[fd].inode; // file_inode will have the inode number for the file where we want to update the current location of the file pointer.
                                               //Here, open_files is a structure containing the value of inode with variable name inode.
	if(!file_inode) {          // If the file is not open (i.e open_files[fd].inode returns 0), it will show the error message and return -1, and set osErrno to E_BAD_FD.
		osErrno = E_BAD_FD; 
		return -1;
	}

	if (open_files[fd].size < offset || offset < 0){  // If offset is larger than the size of the file or negative, it return -1 and set osErrno to E_SEEK_OUT_OF_BOUNDS;
		osErrno = E_SEEK_OUT_OF_BOUNDS;
		return -1;
	}

	
	open_files[fd].pos = offset; // pos will be updated with new offset value

	return open_files[fd].pos;
}

int File_Close(int fd)
{
  dprintf("File_Close(%d):\n", fd);
  if(0 > fd || fd > MAX_OPEN_FILES) {
    dprintf("... fd=%d out of bound\n", fd);
    osErrno = E_BAD_FD;
    return -1;
  }
  if(open_files[fd].inode <= 0) {
    dprintf("... fd=%d not an open file\n", fd);
    osErrno = E_BAD_FD;
    return -1;
  }

  dprintf("... file closed successfully\n");
  open_files[fd].inode = 0;
  return 0;
}

int Dir_Create(char* path)
{
  dprintf("Dir_Create('%s'):\n", path);
  return create_file_or_directory(1, path);
}

int Dir_Unlink(char* path)
{
  /* YOUR CODE */

  dprintf("Dir_Unlink('%s'):\n", path);
  int child_inode;
  char child_fname[MAX_NAME];
  int parent_inode = follow_path(path, &child_inode, child_fname);

  return remove_inode(1, parent_inode, child_inode);
}

int Dir_Size(char* path)
{
  /* YOUR CODE */
  // UNTESTED, but uses nearly the same code as the sample code to get an inode,
  // so it should work
  inode_t* inode;
  if (get_inode_from_path(path, &inode) >= 0)
    return inode->size;

  return 0;
}

int Dir_Read(char* path, void* buffer, int size) {
	int target_inode, sector_number, read_buffer_size, position_in_sector,shift = 0, a,b,c, arr[MAX_SECTORS_PER_FILE],data_availability;
	char File_Name[16],target_sector_buffer[512], directory_storage[512];
	//calling follow_path function to extract target_inode
	follow_path(path, &target_inode, File_Name);
    int offset = target_inode%SECTOR_SIZE;
	assert(offset >= 0 && offset < INODES_PER_SECTOR);
         //target_inode = 13
	//load child inode's sector
	position_in_sector = target_inode/INODES_PER_SECTOR;
	sector_number = INODE_TABLE_START_SECTOR+ position_in_sector;   //3  //

	//Checking whether there is something to read in the target sector. If not return -1 as error
	data_availability = Disk_Read(sector_number, target_sector_buffer);
	if(data_availability < 0)
	   return -1;  //If there is nothing to read in the sector


	inode_t* target_directory = (inode_t*)(target_sector_buffer+offset*sizeof(inode_t));
	read_buffer_size = target_directory->size*sizeof(dirent_t);
	// check the type of inode whether its a diectory or a file
	if(target_directory->type !=1){
		osErrno = E_GENERAL;
		return -1;
	}

	//check the parameter buffer does not exceed the directory size * inode dorectory size
	if(read_buffer_size > size){
		osErrno = E_BUFFER_TOO_SMALL;
		return -1;
	}

	//we need to read all the sectors of the target directories and storing the directories in an array that has data as not equal zero
	for(c = 0; c < MAX_SECTORS_PER_FILE; c++){
		if(target_directory->data[c] != 0){
            arr[c] = target_directory->data[c];
            }
        }

    //traverse through the array and for each sector checking the directories inode. If the directory inode is not zero i.e. 1, then
    dirent_t* target_dir_position;
    for(a=0;a<sizeof(arr);a++){
        Disk_Read(arr[a], directory_storage);
        for(b = 0; b < DIRENTS_PER_SECTOR; b++){
            target_dir_position = (dirent_t*)(directory_storage+b*sizeof(dirent_t));
            if(target_dir_position->inode != 0){
                memcpy(buffer+shift, (void*)target_dir_position, sizeof(dirent_t));
                //shifting to the next slot
                shift = shift+sizeof(dirent_t);
            }
        }
    }

	dprintf("Target directory size = %d\n", target_directory->size);
	return target_directory->size;
}
