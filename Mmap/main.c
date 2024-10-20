#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <semaphore.h>

#define FILE_PATH "mmap_test_file"
#define SIZE (1024L * 1024L)
#define ITERATIONS 100000
#define BLOCK_SIZE 2048
#define SEM_NAME_WRITE "/sem_write_mmap"
#define SEM_NAME_READ "/sem_read_mmap"

double elapsed_time(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
}

void test_mmap_ipc(const char *type) {
    struct timeval start, end;
    pid_t pid;
    int fd;
    size_t i;

    char *buffer = malloc(BLOCK_SIZE);
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    fd = open(FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open failed");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, SIZE) != 0) {
        perror("ftruncate failed");
        close(fd);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    char *addr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_write = sem_open(SEM_NAME_WRITE, O_CREAT, 0666, 1);
    sem_t *sem_read = sem_open(SEM_NAME_READ, O_CREAT, 0666, 0);

    if (sem_write == SEM_FAILED || sem_read == SEM_FAILED) {
        perror("sem_open failed");
        munmap(addr, SIZE);
        close(fd);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        munmap(addr, SIZE);
        close(fd);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        gettimeofday(&start, NULL);
        
        for (i = 0; i < ITERATIONS; i++) {
            sem_wait(sem_read);
            memcpy(buffer, addr + (i % (SIZE - BLOCK_SIZE)), BLOCK_SIZE);
            sem_post(sem_write);
        }
        
        gettimeofday(&end, NULL);

        double elapsed = elapsed_time(start, end);
        double latency = elapsed / ITERATIONS;
        double throughput = ((double)(BLOCK_SIZE * ITERATIONS) / elapsed) * 1e6;
        printf("mmap IPC Results: %d microseconds elapsed, Latency: %.2f microseconds, Throughput: %.2f MB/s\n",
               (int)elapsed, latency, throughput / 1024 / 1024);
        munmap(addr, SIZE);
        close(fd);
        exit(0);
    } else {
        for (i = 0; i < ITERATIONS; i++) {
            sem_wait(sem_write);
            for (int j = 0; j < BLOCK_SIZE; j++) {
                buffer[j] = 'A' + (rand() % 26);
            }
            memcpy(addr + (i % (SIZE - BLOCK_SIZE)), buffer, BLOCK_SIZE);
            sem_post(sem_read);
        }

    }

    munmap(addr, SIZE);
    close(fd);
    free(buffer);
    sem_close(sem_write);
    sem_close(sem_read);
    sem_unlink(SEM_NAME_WRITE);
    sem_unlink(SEM_NAME_READ);
}

int main(void) {
    test_mmap_ipc("mmap-based IPC");
    unlink(FILE_PATH);
    return 0;
}