#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "LibFS.h"

#define BFSZ 256

void usage(char *prog)
{
    printf("USAGE: %s <disk_image_file>\n", prog);
    exit(1);
}

// Displays a given prompt to the user and returns
// the given input string to output, limited by
// size sz
int prompt(char* output, int sz, char* prompt)
{
    printf(prompt);
    if (fgets(output, sz, stdin) == NULL)
    {
        return -1;
    }
    // Remove trailing newline from fgets
    output = strtok(output, "\n");
    return 0;
}

char menu[] = "\nPlease select an operation\n" 
            "1. Create a file\n"
            "2. Read the contents of a file\n"
            "3. Write to a file\n"
            "4. Delete a file\n"
            "5. Create a directory\n"
            "6. See the size of a directory\n"
            "7. Delete a directory\n"
            "8. Exit the filesystem\n\n";

int main(int argc, char *argv[])
{
    char *diskfile;
    int exit = 0;

    if (argc > 2) usage(argv[0]);
    if(argc == 2)
        diskfile = argv[1];
    else 
        diskfile = "default-disk";

    if(FS_Boot(diskfile) < 0) {
        printf("ERROR: can't boot file system from file '%s'\n", diskfile);
        return -1;
    } else printf("file system booted from file '%s'\n", diskfile);

    char in[BFSZ];

    while (!exit)
    {
        if (prompt(in, BFSZ, menu) < 0)
        {
            printf("ERROR: invalid input");
            continue;
        }

        int choice = atoi(in);
        int fd;

        if (choice < 1 || choice > 8)
        {
            printf("ERROR: invalid choice.\n");
            continue;
        }

        switch(choice)
        {
        case 1:
            prompt(in, BFSZ, "Please enter the path of the file to create.\n");
            if (File_Create(in) < 0)
            {
                printf("ERROR: can't create file '%s'\n", in);
                break;
            }
            printf("File '%s' created.\n", in);
            sleep(1);
            break;
        case 2:
            prompt(in, BFSZ, "Please enter the path of the file to read.\n");
            fd = File_Open(in);
            if(fd < 0) {
                printf("ERROR: can't open file '%s'\n", in);
                return -2;
            }

            char buf[BFSZ+1]; int sz;
            do {
                sz = File_Read(fd, buf, BFSZ);
                if(sz < 0) {
                    printf("ERROR: can't read file '%s'\n", in);
                    break;
                }
                printf("\n\nContents of file %s:\n\n", in);
                printf("%s\n\n", buf);
            } while(sz > 0);
            
            File_Close(fd);
            
            if(FS_Sync() < 0) 
            {
                printf("ERROR: can't sync disk '%s'\n", diskfile);
                break;
            }
            sleep(1);
            break;
        case 3:
            prompt(in, BFSZ, "Please enter the path of the file to write to.\n");
            fd = File_Open(in);
            if(fd < 0) 
            {
                printf("ERROR: can't open file '%s'\n", in);
                break;
            }

            prompt(in, BFSZ, "Enter some text to write to the file.\n");
            in[BFSZ] = '\0';

            if(File_Write(fd, in, BFSZ) < 0)
            {
                printf("ERROR: can't write contents to fd=%d\n", fd);
                break;
            }
            else 
                printf("successfully wrote contents to fd=%d\n", fd);
            
            if(File_Close(fd) < 0) 
            {
                printf("ERROR: can't close fd %d\n", fd);
                break;
            }
            else 
                printf("fd %d closed successfully\n", fd);
            sleep(1);
            break;
        case 4:
            prompt(in, BFSZ, "Please enter the path of the file to delete.\n");
            if(File_Unlink(in) < 0)
            {
                printf("ERROR: can't delete file '%s'\n", in);
                break;
            }
            sleep(1);
            break;
        case 5:
            prompt(in, BFSZ, "Please enter the path of the directory to create.\n");
            if (Dir_Create(in) < 0)
            {
                printf("ERROR: can't create directory '%s'\n", in);
                break;
            }
            printf("Directory '%s' created.\n", in);
            sleep(1);
            break;
        case 6:
            prompt(in, BFSZ, "Please enter the path of the directory to query.\n");
            int dirsz = Dir_Size(in);
            if(dirsz < 0)
            {
                printf("ERROR: can't get size of '%s'\n", in);
                break;
            }
            printf("Directory %s contains %d files.\n", in, dirsz);
            sleep(1);
            break;
        case 7:
            prompt(in, BFSZ, "Please enter the path of the directory to delete.\n");
            if(Dir_Unlink(in) < 0)
            {
                printf("ERROR: can't delete directory '%s'\n", in);
                break;
            }
            printf("Directory '%s' successfully deleted.\n", in);
            sleep(1);
            break;
        case 8:
            exit = 1;
            break;
        }
    }

    if(FS_Sync() < 0) {
        printf("ERROR: can't sync file system to file '%s'\n", diskfile);
        return -1;
    } else printf("file system sync'd to file '%s'\n", diskfile);
        
    return 0;
}
