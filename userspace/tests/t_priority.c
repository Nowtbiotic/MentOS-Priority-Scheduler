#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sched.h>

#define CHILDREN 3

int main(void)
{
    int start_pipe[2];
    int ready_pipe[2];
    int result_pipe[2];
    pid_t pids[CHILDREN];
    int priorities[CHILDREN] = {130, 110, 110};
    int results[CHILDREN];
    int expected[CHILDREN] = {1, 2, 0};
    int child_id;
    int status;
    char token;
    char start_tokens[CHILDREN] = {1, 1, 1};
    int i;

    if (pipe(start_pipe) < 0 || pipe(ready_pipe) < 0 || pipe(result_pipe) < 0) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    for (i = 0; i < CHILDREN; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork");
            return EXIT_FAILURE;
        }

        if (pids[i] == 0) {
            close(start_pipe[1]);
            close(ready_pipe[0]);
            close(result_pipe[0]);

            child_id = i;

            if (write(ready_pipe[1], &child_id, sizeof(child_id)) != sizeof(child_id)) {
                exit(EXIT_FAILURE);
            }

            if (read(start_pipe[0], &token, sizeof(token)) != sizeof(token)) {
                exit(EXIT_FAILURE);
            }

            if (write(result_pipe[1], &child_id, sizeof(child_id)) != sizeof(child_id)) {
                exit(EXIT_FAILURE);
            }

            exit(EXIT_SUCCESS);
        }
    }

    close(start_pipe[0]);
    close(ready_pipe[1]);
    close(result_pipe[1]);

    for (i = 0; i < CHILDREN; i++) {
        if (read(ready_pipe[0], &child_id, sizeof(child_id)) != sizeof(child_id)) {
            perror("ready read");
            return EXIT_FAILURE;
        }
    }

    for (i = 0; i < CHILDREN; i++) {
        sched_param_t param;

        param.sched_priority = priorities[i];
        param.period = 0;
        param.deadline = 0;
        param.arrivaltime = 0;
        param.is_periodic = false;

        if (sched_setparam(pids[i], &param) < 0) {
            perror("sched_setparam");
            return EXIT_FAILURE;
        }
    }

    if (write(start_pipe[1], start_tokens, sizeof(start_tokens)) != sizeof(start_tokens)) {
        perror("start write");
        return EXIT_FAILURE;
    }

    for (i = 0; i < CHILDREN; i++) {
        if (read(result_pipe[0], &results[i], sizeof(results[i])) != sizeof(results[i])) {
            perror("result read");
            return EXIT_FAILURE;
        }
    }

    for (i = 0; i < CHILDREN; i++) {
        if (waitpid(pids[i], &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            printf("FAIL: Child %d did not exit successfully\n", i);
            return EXIT_FAILURE;
        }
    }

    printf("Observed child order: %d, %d, %d\n",
           results[0], results[1], results[2]);

    printf("Expected child order: %d, %d, %d\n",
           expected[0], expected[1], expected[2]);

    for (i = 0; i < CHILDREN; i++) {
        if (results[i] != expected[i]) {
            printf("FAIL: Priority ordering is incorrect\n");
            return EXIT_FAILURE;
        }
    }

    printf("PASS: Priority ordering and equal-priority FIFO verified\n");

    close(start_pipe[1]);
    close(ready_pipe[0]);
    close(result_pipe[0]);

    return EXIT_SUCCESS;
}
