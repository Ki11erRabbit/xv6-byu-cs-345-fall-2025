//
// Created by Alec Davis on 9/8/25.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int add_path(char *path, int current_path_len, char *add) {
    int add_len = strlen(add);
    memmove(path + current_path_len, add, add_len);

    path[current_path_len + add_len] = '\0';
    return current_path_len + add_len;
}

int add_slash(char *path, int current_path_len) {
    return add_path(path, current_path_len, "/");
}

void remove_path(char *path, int current_path_len) {
    path[current_path_len] = '\0';
}

int last_segment_start(char *path, int current_path_len) {
    int index = current_path_len - 1;
    while (index >= 0 && path[index] != '/') {
         index--;
    }
    index++;
    return index;
}

int explore_directory(char *path, int current_path_len, char *needle) {
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(2, "can't open %s\n", path);
        return -1;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "can't fstat %s\n", path);
        close(fd);
        return -1;
    }

    switch (st.type) {
        case T_FILE:
            int offset = last_segment_start(path, current_path_len);
            if (strcmp(path + offset, needle) == 0) {
                add_path(path, current_path_len, de.name);
                printf("%s\n", path);
                remove_path(path, current_path_len);
            }
            break;
        case T_DIR:
            int with_slash = add_slash(path, current_path_len);
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                    continue;
                }
                int new_len = add_path(path, current_path_len, de.name);
                int result = explore_directory(path, new_len, needle);
                if (result < 0)
                    return result;
                remove_path(path, with_slash);
            }
            remove_path(path, current_path_len);
            break;
    default:
        break;
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {

    if (argc <= 2) {
        printf("Usage: find <path>\n");
        exit(1);
    }

    char path[512] = { 0 };

    memmove(path, argv[1], strlen(argv[1]));
    path[strlen(argv[1])] = '/';
    int result = explore_directory(path, strlen(argv[1]) + 1, argv[2]);


    exit(result);
}
