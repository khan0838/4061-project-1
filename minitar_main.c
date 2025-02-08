#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }
    else{
        file_list_t files;
        file_list_init(&files);
        char *archive_name = NULL;

        //checking if the user is using the -f command to add an archive file
        if(strcmp(argv[2], "-f") == 0){
            archive_name = argv[3];
        }
        else{
            printf("Error: no -f input for archive name");
            file_list_clear(&files);
            return 0;
        }
        int i = 4;
        files.size ++;
        //initializing the linked list of files
        while(i < argc){
            file_list_add(&files, argv[i]);
            i++;
        }

        if(strcmp(argv[1], "-c") == 0){


        }
        else if(strcmp(argv[1], "-a") == 0){

        }
        else if(strcmp(argv[1], "-t") == 0){

        }
        else if(strcmp(argv[1], "-u") == 0){

        }
        else if(strcmp(argv[1], "-x") == 0){

        }
        else{
            printf("Error: no operation given");
            file_list_clear(&files);
            return 0;
        }



        file_list_clear(&files);
        return 0;
    }
}
