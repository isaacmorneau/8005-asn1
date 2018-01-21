#include <getopt.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <cmath>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include <gmpxx.h>

#define OMP_MODE 1
#define THREAD_MODE 2
#define PROC_MODE 3

static inline void print_help() {
    puts("usage:\n"
            "\t -[-o]penmp                    - run task with openmp\n"
            "\t -[-p]rocess                   - run task with processes\n"
            "\t -[-t]hread                    - run task with threads\n"
            "\t -[-b]ranch      <default 100> - number of times to calculate pi\n"
            "\t -[-i]tterations <default 1>   - number of times to run to get time samples\n"
            "\t -[-h]elp                      - this message\n"
            );
};
static int mode = 0;
static int branches = 100;
static int splits = get_nprocs();


void agm(mpf_class& rop1, mpf_class& rop2, const mpf_class& op1,
        const mpf_class& op2) {
    rop1 = (op1 + op2) / 2;
    rop2 = op1 * op2;
    mpf_sqrt(rop2.get_mpf_t(), rop2.get_mpf_t());
}

void task() {
    mpf_set_default_prec(300000);
    mpf_class x0, y0, resA, resB, Z;

    x0 = 1;
    y0 = 0.5;
    Z  = 0.25;
    mpf_sqrt(y0.get_mpf_t(), y0.get_mpf_t());

    int n = 1;
    for (int i = 0; i < 16; i++) {
        agm(resA, resB, x0, y0);
        Z -= n * (resA - x0) * (resA - x0);
        n *= 2;

        agm(x0, y0, resA, resB);
        Z -= n * (x0 - resA) * (x0 - resA);
        n *= 2;
    }

    x0 = x0 * x0 / Z;
    //only print this if you want to see pi a bunch of times
    //gmp_printf ("%.100000Ff\n", x0.get_mpf_t());
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

void run_task_proc() {
    for (int i = 0; i < branches/splits; ++i)
        task();
    exit(0);
}

struct timespec process() {
    struct timespec start, end;
    //start the clock
    clock_gettime(CLOCK_REALTIME, &start);
    for (int i = 0; i < splits; ++i) {
        switch(fork()) {
            case 0:
                //child
                run_task_proc();
                return {-1,-1};
            case -1:
                //error
                perror("fork");
                return {-1,-1};
            default:
                //parent
                break;
        }
    }
    //spinlock waiting for children to die
    while (wait(0) != -1);
    //stop the clock
    clock_gettime(CLOCK_REALTIME, &end);
    return {end.tv_sec - start.tv_sec, end.tv_nsec - start.tv_nsec};
}


void *run_task_thread(void * ) {
    for (int i = 0; i < branches/splits; ++i)
        task();
    return 0;
}
struct timespec threads() {
    struct timespec start, end;
    pthread_t * th_id = (pthread_t *)malloc(sizeof(pthread_t)*splits);
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    //start the clock
    clock_gettime(CLOCK_REALTIME, &start);
    for(int i = 0; i < splits; ++i) {
        pthread_create(th_id + splits, &attr, run_task_thread, 0);
    }
    for(int i = 0; i < splits; ++i) {
        void * ret;
        pthread_join(th_id[splits], &ret);
    }
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
            {"branch",      no_argument, 0, 'b' },
            {"openmp",      no_argument, 0, 'o' },
            {"process",     no_argument, 0, 'p' },
            {"thread",      no_argument, 0, 't' },
            {"itterations", no_argument, 0, 'i' },
            {"help",        no_argument, 0, 'h' },
            {0,             0,           0, 0 }
        };

        c = getopt_long(argc, argv, "optb:i:h",
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
                mode = PROC_MODE;
                break;
            case 't':
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

    printf("%s seconds:\n", mode == OMP_MODE ? "OpenMP" : mode == PROC_MODE ? "Processes" : "Threads");
    while (itterations--) {
        switch (mode) {
            case OMP_MODE:
                elapsed = openmp();
                break;
            case THREAD_MODE:
                elapsed = threads();
                break;
            case PROC_MODE:
                elapsed = process();
                if (elapsed.tv_sec == -1 && elapsed.tv_nsec == -1)
                    return 0;
                break;
        }
        printf("%g\n", elapsed.tv_sec + (elapsed.tv_nsec * 1e-9));
    }
    return 0;
}
