#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }
    file_list_t files;
    file_list_init(&files);
    char *archive_name = NULL;

    // checking if the user is using the -f command to add an archive file
    if (strcmp(argv[2], "-f") == 0) {
        archive_name = argv[3];
    } else {
        printf("Error: no -f input for archive name\n");
        file_list_clear(&files);
        return 1;
    }

    // initializing the linked list of files
    for (int i = 4; i < argc; i++) {
        file_list_add(&files, argv[i]);
    }

    // switch case for each of the command options
    if (strcmp(argv[1], "-c") == 0) {
        if (create_archive(archive_name, &files) == 0) {
            file_list_clear(&files);
            return 0;
        } else {
            perror("Error: something went with creating archive\n");
            file_list_clear(&files);
            return 1;
        }

    } else if (strcmp(argv[1], "-a") == 0) {
        if (append_files_to_archive(archive_name, &files) == 0) {
            file_list_clear(&files);
            return 0;
        } else {

            file_list_clear(&files);
            return 1;
        }
    }

    else if (strcmp(argv[1], "-t") == 0) {
        if (get_archive_file_list(archive_name, &files) == 0) {
            // traverses the linked list and prints out the name of the files
            node_t *current = files.head;
            while (current != NULL) {
                printf("%s\n", current->name);
                current = current->next;
            }
            file_list_clear(&files);
            return 0;
        } else {
            perror("Error: something went with getting archive\n");
            file_list_clear(&files);
            return 1;
        }
    }

    else if (strcmp(argv[1], "-u") == 0) {
        file_list_t oldFiles;
        file_list_init(&oldFiles);

        //access old files
        if(get_archive_file_list(archive_name, &oldFiles) == -1){
            perror("Failed to call get method");
            file_list_clear(&files);
            file_list_clear(&oldFiles);
            return 1;
        }

        if(file_list_is_subset(&files, &oldFiles) == 0){
            printf("Error: One or more of the specified files is not already present in archive");
            file_list_clear(&files);
            file_list_clear(&oldFiles);

            return 1;
        }
        else{
            if(append_files_to_archive(archive_name, &files) == -1){
                perror("could not append file");
                file_list_clear(&files);
                file_list_clear(&oldFiles);
                return 1;

            }
            file_list_clear(&files);
            file_list_clear(&oldFiles);
            return 0;

        }
        file_list_clear(&files);
        file_list_clear(&oldFiles);
        return 0;


    }

    else if (strcmp(argv[1], "-x") == 0) {
        if (extract_files_from_archive(archive_name) == 0) {
            file_list_clear(&files);
            return 0;
        } else {
            perror("Error: something went with extracting archive\n");
            file_list_clear(&files);
            return 1;
        }
    }

    else {
        printf("Error: no operation given\n");
        file_list_clear(&files);
        return 1;
    }
}
