//
// Created by Alec Davis on 9/8/25.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int parent_to_child_pipe[2];
	int child_to_parent_pipe[2];
    pipe(parent_to_child_pipe);
    pipe(child_to_parent_pipe);
    if (fork() == 0) {
        char byte = 0;
        read(parent_to_child_pipe[1], &byte, 1);
        fprintf(2, "%d: received ping\n", getpid());
        write(child_to_parent_pipe[0], &byte, 1);
    } else {
        char byte = 1;
        write(parent_to_child_pipe[0], &byte, 1);
        read(child_to_parent_pipe[1], &byte, 1);
		wait((void *)0);
        fprintf(2, "%d: received pong\n", getpid());
    }
    exit(0);
}