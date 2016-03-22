#include "libasync.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static float run_start;

static void greeting(void *arg)
{
    int static i = 0;
    i += 5;
    fprintf(stderr, "Hi! %d\n", i++);
}

static void schedule_tasks2(void *arg)
{
    fprintf(stderr, "SCHEDULE TASKS START (%lf)\n",
            ((float)clock() / CLOCKS_PER_SEC) - run_start);
    async_p async = arg;
    for (size_t i = 0; i < (8 * 1024); i++) {
        Async.run(async, greeting, NULL);
        printf("wrote task %lu\n", i);
    }
    Async.run(async, greeting, NULL);
    Async.signal(async);
    printf("signal finish at %lf\n",
           ((float)clock() / CLOCKS_PER_SEC) - run_start);
}

static void schedule_tasks(void *arg)
{
    fprintf(stderr, "SCHEDULE TASKS START (%lf)\n",
            ((float)clock() / CLOCKS_PER_SEC) - run_start);
    async_p async = arg;
    for (size_t i = 0; i < (8 * 1024); i++) {
        Async.run(async, greeting, NULL);
    }
    Async.run(async, schedule_tasks2, async);
}

int main(void)
{
    fprintf(stderr, "%d Testing Async library\n", getpid());
    /* create the thread pool with a single threads.
     * the callback is optional (we can pass NULL)
     */
    async_p async = Async.create(32);
    if (!async) {
        perror("ASYNC creation failed");
        exit(1);
    }
    /* send a task */
    float run_start = (float)clock() / CLOCKS_PER_SEC;
    Async.run(async, schedule_tasks, async);
    /* wait for all tasks to finish, closing the threads,
     * clearing the memory
     */
    Async.wait(async);
    fprintf(stderr, "Finish (%lf) ms\n",
            (((float)clock() / CLOCKS_PER_SEC) - run_start) * 1000);
    return 0;
}
