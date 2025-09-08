//
// Created by Alec Davis on 9/8/25.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int the_pipe[2];
    pipe(the_pipe);
    if (fork() == 0) {
        char byte = 0;
        read(the_pipe[1], &byte, 1);
        printf("%d: received ping\n", getpid());
        write(the_pipe[0], &byte, 1);
    } else {
        char byte = 1;
        write(the_pipe[0], &byte, 1);
        read(the_pipe[1], &byte, 1);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}