#include <getopt.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#define OMP_MODE 1
#define THREAD_MODE 2
#define PROC_MODE 3

static inline void print_help() {
    puts("usage:\n"
            "\t -[-o]penmp                      - run task with openmp\n"
            "\t -[-p]rocess     <number to use> - run task with manual processes\n"
            "\t -[-t]hread      <number to use> - run task with manual threads\n"
            "\t -[-b]ranch      <default 4>     - number of times to run the task\n"
            "\t -[-i]tterations <default 1>     - number of times to check the run time\n"
            "\t -[-h]elp                        - this message\n"
            );
};
static int mode = 0;
static int branches = 4;
static int splits = 0;
sem_t * sem_run = 0;
sem_t * sem_ready = 0;

void task() {
    volatile int a = 0xDEAD;
    volatile int b = 0XBEEF;
    volatile int c = 0XC0FFE;
    for(int i = 0; i < 1000; ++i){
        c = b * i;
        for(int j = 0; j < 1000; ++j){
            b = a * j;
        }
    }
}

//openmp testing
struct timespec openmp() {
    struct timespec start, end;
    clock_gettime(CLOCK_REALTIME, &start);
#pragma omp parallel for
    for (int i = 0; i < branches; ++i)
        task();
    clock_gettime(CLOCK_REALTIME, &end);
    return {end.tv_sec - start.tv_sec, end.tv_nsec - start.tv_nsec};
}


//threads testing
void *run_task(void * ) {
    sem_post(sem_ready);
    sem_wait(sem_run);
    for (int i = 0; i < branches/splits; ++i)
        task();
    sem_post(sem_run);
    return 0;
}

struct timespec threads() {
    struct timespec start, end;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for(int i = 0; i < splits; ++i) {
        pthread_t th_id;
        pthread_create(&th_id, &attr, run_task, 0);
    }

    //wait for all threads to signal ready
    for(int i = 0; i < splits; ++i)
        sem_wait(sem_ready);

    //start the clock and all the threads
    clock_gettime(CLOCK_REALTIME, &start);
    for(int i = 0; i < splits; ++i)
        sem_post(sem_run);

    //wait for all the threads to signal finishing
    for(int i = 0; i < splits; ++i)
        sem_wait(sem_run);
    //stop the clock
    clock_gettime(CLOCK_REALTIME, &end);

    pthread_attr_destroy(&attr);

    return {end.tv_sec - start.tv_sec, end.tv_nsec - start.tv_nsec};
}

int main(int argc, char ** argv) {

    struct timespec elapsed;
    int itterations = 1;
    while (1) {
        int option_index = 0;
        static int c = 0;
        static struct option long_options[] = {
            {"branch",      required_argument, 0, 'b' },
            {"openmp",      no_argument,       0, 'o' },
            {"process",     required_argument, 0, 'p' },
            {"thread",      required_argument, 0, 't' },
            {"itterations", required_argument, 0, 'i' },
            {"help",        no_argument,       0, 'h' },
            {0,             0,                 0, 0 }
        };

        c = getopt_long(argc, argv, "op:t:b:i:h",
                long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'b':
                branches = atoi(optarg);
                break;
            case 'i':
                itterations = atoi(optarg);
                break;
            case 'o':
                mode = OMP_MODE;
                break;
            case 'p':
                splits = atoi(optarg);
                mode = PROC_MODE;
                break;
            case 't':
                splits = atoi(optarg);
                mode = THREAD_MODE;
                break;
            case 'h':
            case '?':
            default:
                print_help();
                return 1;
        }
    }
    if (!mode) {
        print_help();
        return 1;
    }

    sem_run = sem_open("sync_run", O_CREAT, 0644, 0);
    sem_ready = sem_open("sync_ready", O_CREAT, 0644, 0);
    if (sem_run == SEM_FAILED)  {
        perror("sem_open");
        return 2;
    }
    if (sem_ready == SEM_FAILED)  {
        perror("sem_open2");
        return 2;
    }

    while (itterations--) {
        switch (mode) {
            case OMP_MODE:
                elapsed = openmp();
                break;
            case THREAD_MODE:
                elapsed = threads();
                break;
            case PROC_MODE:
                break;
        }
        printf("%lf seconds\n", elapsed.tv_sec + (elapsed.tv_nsec * 0.0000000001));
    }

    sem_close(sem_ready);
    sem_close(sem_run);
    return 0;
}
