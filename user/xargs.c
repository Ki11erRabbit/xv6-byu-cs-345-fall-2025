//
// Created by Alec Davis on 9/8/25.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void lines(char *input, int input_length, char ***output, int *output_length) {
    int offset = 0;
    int offset_length = 0;
    char *buffer[512] = {0};
    int buffer_length = 0;
    do {
        if (input[offset + offset_length] == '\n' || offset + offset_length == input_length) {
            buffer[buffer_length] = malloc(sizeof(char) * (offset_length + 1));
            memcpy(buffer[buffer_length], input + offset, offset_length * sizeof(char));
            buffer[buffer_length][offset_length] = '\0';
            buffer[buffer_length][offset + offset_length] = '\0';
            buffer_length++;
            offset = offset + offset_length + 1;
            offset_length = 1;
        }
        offset_length++;
    } while (offset + offset_length < input_length);
    *output = malloc(sizeof(char*) * buffer_length);
    *output_length = buffer_length;
    for (int i = 0; i < buffer_length; i++) {
        (*output)[i] = buffer[i];
    }
    return;
}



int main(int argc, char *argv[]){
    if (argc < 2) {
        fprintf(2, "Usage: xargs <..args>\n");
        exit(1);
    }

    char input[1024] = {0};
    read(0, input, 1024);
    char **args;
    int args_length = 0;

    lines(input, strlen(input), &args, &args_length);
    int all_args_length = args_length + (argc);
    char **all_args = malloc(sizeof(char*) * all_args_length);
    all_args[all_args_length] = (char *)0;

    for (int i = 1; i < argc; i++) {
        all_args[i - 1] = argv[i];
    }

    for (int i = 0; i < args_length; i++) {
        all_args[argc + i - 1] = args[i];
    }

    exec(argv[1], all_args);
    exit(0);
}