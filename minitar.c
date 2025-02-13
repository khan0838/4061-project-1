#include "minitar.h"

#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 128
#define BLOCK_SIZE 512

// Constants for tar compatibility information
#define MAGIC "ustar"

// Constants to represent different file types
// We'll only use regular files in this project
#define REGTYPE '0'
#define DIRTYPE '5'

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *) header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100);    // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o",
             stat_buf.st_mode & 07777);    // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid);    // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid);       // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32);    // Owner name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid);    // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid);        // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32);    // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o",
             (unsigned) stat_buf.st_size);    // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o",
             (unsigned) stat_buf.st_mtime);    // Modification time, 0-padded octal
    header->typeflag = REGTYPE;                // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6);          // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2);          // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o",
             major(stat_buf.st_dev));    // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o",
             minor(stat_buf.st_dev));    // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];

    struct stat stat_buf;
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    off_t file_size = stat_buf.st_size;
    if (nbytes > file_size) {
        file_size = 0;
    } else {
        file_size -= nbytes;
    }

    if (truncate(file_name, file_size) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}

int create_archive(const char *archive_name, const file_list_t *files) {
    char err_msg[MAX_MSG_LEN];
    int archive_fd = open(archive_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (archive_fd == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to create archive %s", archive_name);
        perror(err_msg);
        return -1;
    }

    const node_t *current = files -> head;
    while (current != NULL) {
        int file_fd = open(current -> name, O_RDONLY);
        if (file_fd == -1) {
            snprintf(err_msg, MAX_MSG_LEN, "Failed to open file %s", current -> name);
            perror(err_msg);
            close(archive_fd);
            return -1;
        }

        tar_header header;
        memset(&header, 0, sizeof(header));
        if (fill_tar_header(&header, current -> name) == -1) {
            close(file_fd);
            close(archive_fd);
            return -1;
        }

        if (write(archive_fd, &header, BLOCK_SIZE) != BLOCK_SIZE) {
            snprintf(err_msg, MAX_MSG_LEN, "Failed to write header for %s", current -> name);
            perror(err_msg);
            close(file_fd);
            close(archive_fd);
            return -1;
        }

        char buffer[BLOCK_SIZE];
        ssize_t bytes_rd;
        while ((bytes_rd = read(file_fd, buffer, BLOCK_SIZE)) > 0) {
            if (bytes_rd < BLOCK_SIZE) {
                memset(buffer + bytes_rd, 0, BLOCK_SIZE - bytes_rd);
            }

            if (write(archive_fd, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
                snprintf(err_msg, MAX_MSG_LEN, "Failed to write content for %s", current -> name);
                perror(err_msg);
                close(file_fd);
                close(archive_fd);
                return -1;
            }
        }

        if (bytes_rd == -1) {
            snprintf(err_msg, MAX_MSG_LEN, "Failed to read from %s", current -> name);
            perror(err_msg);
            close(file_fd);
            close(archive_fd);
            return -1;
        }

        close(file_fd);
        current = current -> next;
    }

    char zeros[BLOCK_SIZE * NUM_TRAILING_BLOCKS] = {0};
    if (write(archive_fd, zeros, BLOCK_SIZE * NUM_TRAILING_BLOCKS) != BLOCK_SIZE * NUM_TRAILING_BLOCKS) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to write end markers to %s", archive_name);
        perror(err_msg);
        close(archive_fd);
        return -1;
    }

    close(archive_fd);
    return 0;

}

int append_files_to_archive(const char *archive_name, const file_list_t *files) {
    char err_msg[MAX_MSG_LEN];

    FILE *archive = fopen(archive_name, "ab");
    if (archive == NULL) {
        perror("Error opening archive");
        return -1;
    }

    if (remove_trailing_bytes(archive_name, 2 * BLOCK_SIZE) != 0) {
        fclose(archive);
        return -1;
    }

    // if (fseek(archive, 0, SEEK_END) != 0) {
    //     perror("failed to seek to end of archive");
    //     fclose(archive);
    //     return -1;
    // }

    node_t *current = files->head;
    while (current != NULL) {
        // writes the header of file to archive
        tar_header *header = (tar_header *) malloc(BLOCK_SIZE);
        if (fill_tar_header(header, current->name) != 0) {
            fclose(archive);
            free(header);
            return -1;
        }

        if (fwrite(header, 1, BLOCK_SIZE, archive) != BLOCK_SIZE) {
            perror("failed to write header to archive");
            fclose(archive);
            free(header);
            return -1;
        }

        // open the file in order to append to archive
        FILE *current_file = fopen(current->name, "rb");
        if (current_file == NULL) {
            perror("failed to open current file for reading");
            fclose(archive);
            free(header);

            return -1;
        }
        free(header);

        // read the file contents and write it to the archive in blocks

        char buffer[BLOCK_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, current_file)) > 0) {
            // If we read a partial block, pad remainder with zeros
            if (bytes_read < BLOCK_SIZE) {
                memset(buffer + bytes_read, 0, BLOCK_SIZE - bytes_read);
            }

            size_t bytes_written = fwrite(buffer, 1, BLOCK_SIZE, archive);
            if (bytes_written != BLOCK_SIZE) {
                snprintf(err_msg, MAX_MSG_LEN, "Failed to write content for %s", current->name);
                perror(err_msg);
                fclose(current_file);
                fclose(archive);
                return -1;
            }
        }

        // if there was an error reading the file
        if (bytes_read == -1) {
            perror("failed to read from file");
            fclose(current_file);
            fclose(archive);
            return -1;
        }
        fclose(current_file);
        current = current->next;
    }

    // create 2 blocks that are zero out for the footer

    char zero_block[BLOCK_SIZE] = {0};
    if (fwrite(zero_block, 1, BLOCK_SIZE, archive) != BLOCK_SIZE) {
        perror("failed to write 0 block to end of file");
        fclose(archive);
        return -1;
    }
    if (fwrite(zero_block, 1, BLOCK_SIZE, archive) != BLOCK_SIZE) {
        perror("failed to write 0 block to end of file");
        fclose(archive);
        return -1;
    }
    fclose(archive);
    return 0;
}
int string_length(const char *str) {
    int length = 0;

    // Loop through each character until we reach the null terminator
    while (str[length] != '\0') {
        length++;
    }

    return length;
}

/*
 * Add the name of each file contained in the archive identified by 'archive_name'
 * to the 'files' list.
 * NOTE: This function is most obviously relevant to implementing minitar's list
 * operation, but think about how you can reuse it for the update operation.
 * This function should return 0 upon success or -1 if an error occurred.
 */
int get_archive_file_list(const char *archive_name, file_list_t *files) {
    char buffer[BLOCK_SIZE];
    FILE *archive = fopen(archive_name, "rb");    // Open the archive for reading binary data
    if (archive == NULL) {
        perror("Error opening archive");
        return -1;
    }

    long bytes_read;
    while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, archive)) > 0) {
        tar_header *header = malloc(BLOCK_SIZE);

        memcpy(header, buffer, BLOCK_SIZE);
        if (file_list_add(files, header->name) == -1) {
            perror("Failed to add file to list");
            return -1;
        }

        int file_size;
        if (sscanf(header->size, "%o", &file_size) == 0) {
            printf("something went wrong with reading size");
            fclose(archive);
            free(header);
            return -1;
        }    // Convert size from octal to decimal
        int total_blocks =
            (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;    // Ceiling division for blocks

        if (fseek(archive, total_blocks * BLOCK_SIZE, SEEK_CUR) != 0) {
            perror("something went wrong");
            fclose(archive);
            free(header);
            return -1;
        }

        free(header);
    }
    fclose(archive);
    return 0;
}

/*
 * Write each file contained within the archive identified by 'archive_name'
 * as a new file to the current working directory.
 * If there are multiple versions of the same file present in the archive,
 * then only the most recently added version should be present as a new file
 * at the end of the extraction process.
 * This function should return 0 upon success or -1 if an error occurred.
 */
int extract_files_from_archive(const char *archive_name) {
    char buffer[BLOCK_SIZE];
    FILE *archive = fopen(archive_name, "rb");    // Open the archive for reading binary data
    if (archive == NULL) {
        perror("Error opening archive");
        return -1;
    }

    long bytes_read;
    while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, archive)) > 0) {
        tar_header *header = malloc(BLOCK_SIZE);
        memcpy(header, buffer, BLOCK_SIZE);

        int file_size;
        if (sscanf(header->size, "%o", &file_size) == 0) {
            printf("something went wrong with reading size");
            fclose(archive);
            free(header);
            return -1;
        }    // Convert size from octal to decimal
        // int total_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

        // OPEN FILE FOR WRITING - WRITE IN byte_read(read from archive) to new files
        //

        free(header);
    }
    fclose(archive);

    return 0;
}

