#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./%s <directory path> <filename>\n", argv[0]);
        return 1;
    }

    char *dir_path = argv[1];
    char *target_file = argv[2];
    DIR *dir;
    struct dirent *entry;
    int found = 0;
    char buffer[BUF_SIZE];
    ssize_t bytes_read;

    // Construct full path to file
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, target_file);

    // Open the directory
    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("opendir");
        return 1;
    }

    // Search for the file in the directory
    while ((entry = readdir(dir)) != NULL) {
        printf("entry->d_name: %s\n", entry->d_name);
        if ((strcmp(entry->d_name, target_file) == 0) && (entry->d_type == DT_REG)) {
	    printf("Match found!\n");
            found = 1;
            break;
        }
    }
    closedir(dir);

    if (!found) {
        printf("File not found!\n");
        return 0;
    }
    printf("File found\n");

    // Open the file using system call
    int fd = open(full_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Read and write the content to stdout
    while ((bytes_read = read(fd, buffer, BUF_SIZE)) > 0) {
        if (write(STDOUT_FILENO, buffer, bytes_read) != bytes_read) {
            perror("write");
            close(fd);
            return 1;
        }
    }

    if (bytes_read < 0) {
        perror("read");
    }

    close(fd);
    return 0;
}

