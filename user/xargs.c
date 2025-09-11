//
// Created by Alec Davis on 9/8/25.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"


void mark_newlines(char *input, int input_length, char **output) {
	*output = malloc(input_length + 2); // add null terminator and extra null to know when to stop
	(*output)[input_length + 2] = '\0';
	(*output)[input_length + 1] = '\0';
	memcpy(*output, input, input_length);
	for (int i = 0; i < input_length; i++) {
		if ((*output)[i] == '\n') {
			(*output)[i] = '\0';
		} else if ((*output)[i] == '\0') {
			break;
		}
	}
}

void exec_for_each(char **argv, int argc, char *lines) {
	char **args = malloc((argc + 1) * sizeof(char *));
	args[argc + 1] = (char *)0;

	memcpy(args, argv + 1, (argc - 1) * sizeof(char *));

	while (1) {
		args[argc - 1] = lines;
		if (fork() == 0) {
			exec(args[0], args);
		} else {
			wait((void*)0);
		}
		lines += strlen(lines) + 1;
		if (lines[0] == '\0') {
			break;
		}
	}
}



int main(int argc, char *argv[]){
    if (argc < 2) {
        fprintf(2, "Usage: xargs <..args>\n");
        exit(1);
    }

    char input[1024] = {0};
    int total_read = 0;
    while (total_read < 1024) {
        int result = read(0, input + total_read, 1024 - total_read);
        if (result <= 0) {
            break;
        }
        total_read += result;
    }
	char *lines = (char*)0;
	mark_newlines(input, total_read, &lines);

	exec_for_each(argv, argc, lines);

    exit(0);
}
