#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LibFS.h"

void usage(char *prog)
{
  printf("USAGE: %s <disk_image_file>\n", prog);
  exit(1);
}

int main(int argc, char *argv[])
{
  char *diskfile, *path;
  
  if (argc != 2 && argc!= 3) usage(argv[0]);
  if(argc == 3) 
  {  diskfile=argv[1]; path = argv[2];
  }
  else {diskfile = "default-disk";path=argv[1];}

  if(FS_Boot(argv[1]) < 0) {
    printf("ERROR: can't boot file system from file '%s'\n", diskfile);
    return -1;
  } else printf("file system booted from file '%s'\n", diskfile);

  char* fn;

  fn = path;

  if(File_Create(fn) < 0)
  printf("ERROR: can't create file '%s'\n", fn);
  else 
  printf("file '%s' created successfully\n", fn);

  
  if(FS_Sync() < 0) 
  {
    printf("ERROR: can't sync file system to file '%s'\n", argv[1]);
    return -1;
  } 
  else printf("file system sync'd to file '%s'\n", argv[1]);
    
  return 0;
}
