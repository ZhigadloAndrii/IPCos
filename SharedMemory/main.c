#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <semaphore.h>

#define SHM_SIZE 2048
#define ITERATIONS 100000
#define SEM_WRITE "/sem_writer"
#define SEM_READ "/sem_reader"

double elapsed_time(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
}

void run_writer(int shmid, sem_t *sem_write, sem_t *sem_read) {
    char *shared_mem = (char *)shmat(shmid, NULL, 0);
    if (shared_mem == (char *)-1) {
        perror("Error attaching shared memory in writer");
        exit(EXIT_FAILURE);
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (size_t i = 0; i < ITERATIONS; i++) {
        for (int j = 0; j < SHM_SIZE; j++) {
            shared_mem[j] = 'A' + (rand() % 26);
        }
        sem_post(sem_read);
        sem_wait(sem_write);
    }

    gettimeofday(&end, NULL);

    double elapsed = elapsed_time(start, end);
    double latency = elapsed / ITERATIONS;
    double throughput = ((double)(SHM_SIZE * ITERATIONS) / elapsed) * 1e6;
    printf("Shared Memory IPC: %d microseconds elapsed, Latency: %.2f microseconds, Throughput: %.2f MB/s\n",
           (int)elapsed, latency, throughput / (1024 * 1024));

    if (shmdt(shared_mem) == -1) {
        perror("Error detaching shared memory in writer");
        exit(EXIT_FAILURE);
    }
}

void run_reader(int shmid, sem_t *sem_write, sem_t *sem_read) {
    char *shared_mem = (char *)shmat(shmid, NULL, 0);
    if (shared_mem == (char *)-1) {
        perror("Error attaching shared memory in reader");
        exit(EXIT_FAILURE);
    }

    char local_buffer[SHM_SIZE];

    for (size_t i = 0; i < ITERATIONS; i++) {
        sem_wait(sem_read);
        memcpy(local_buffer, shared_mem, SHM_SIZE);
        sem_post(sem_write);
    }

    if (shmdt(shared_mem) == -1) {
        perror("Error detaching shared memory in reader");
        exit(EXIT_FAILURE);
    }
}

int main(void) {
    int shmid = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("Error creating shared memory");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_write = sem_open(SEM_WRITE, O_CREAT, 0644, 0);
    sem_t *sem_read = sem_open(SEM_READ, O_CREAT, 0644, 0);
    if (sem_write == SEM_FAILED || sem_read == SEM_FAILED) {
        perror("Error initializing semaphores");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Error forking process");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        run_writer(shmid, sem_write, sem_read);
        exit(0);
    } else {
        run_reader(shmid, sem_write, sem_read);
        wait(NULL);
    }

    shmctl(shmid, IPC_RMID, NULL);
    sem_close(sem_write);
    sem_close(sem_read);
    sem_unlink(SEM_WRITE);
    sem_unlink(SEM_READ);

    return 0;
}