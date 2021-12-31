#include <stdlib.h>
#include <stdio.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/signal.h>
#include <stdbool.h>
#include <time.h>

int buffer_size, shm_id, count, empty;

typedef union semaphore_t
{
    int val;               /* Value for SETVAL */
    struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO */
    void *__pad;
} semaphore_t;

key_t generate_key(int seed)
{
    return ftok("KeyFile", seed);
}

void get_buffer_size()
{
    int msg_id = msgget(generate_key(-1), IPC_CREAT | 0666);
    if (msg_id == -1)
    {
        perror("Error getting message queue");
        exit(-1);
    }

    if (msgrcv(msg_id, &buffer_size, sizeof(int), 0, !IPC_NOWAIT) == -1)
    {
        perror("Error getting buffer size");
        exit(-1);
    }

    if (msgctl(msg_id, IPC_RMID, NULL) == -1)
    {
        perror("Error removing message queue");
        exit(-1);
    }

    printf("Buffer size %d received\n", buffer_size);
}

int get_semaphore(key_t sem_key)
{
    int sem_id = semget(sem_key, 1, IPC_CREAT | 0666);

    if (sem_id == -1)
    {
        perror("Error getting semaphore");
        exit(-1);
    }

    return sem_id;
}

struct sembuf sem_op;
void do_sem_op(int increment, int sem_id)
{
    sem_op.sem_num = 0;
    sem_op.sem_op = increment;
    sem_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem_id, &sem_op, 1) == -1)
    {
        perror("Error doing semaphore operation");
        exit(-1);
    }
}

void down(int sem_id)
{
    do_sem_op(-1, sem_id);
}

void up(int sem_id)
{
    do_sem_op(1, sem_id);
}
void consume_item(int *index, int *buffer)
{
    while (buffer[*index] == -1 && *index < buffer_size)
        ++(*index);
    if (*index < buffer_size)
    {
        printf("Consumer: Consuming item %d\n", buffer[*index]);
        buffer[*index] = -1;
    }
    *index = (*index + 1) % buffer_size;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Incorrect usage, to run use: %s <consumption rate>\n", argv[0]);
        exit(-1);
    }

    int period = (1e9 - 1) / atoi(argv[1]);
    // This is the number of nanoseconds between each consumption

    get_buffer_size();

    shm_id = shmget(generate_key(0), buffer_size * sizeof(int), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Error getting shared memory");
        exit(-1);
    }

    int *buffer = shmat(shm_id, NULL, 0);
    // This is the shared memory segment that will be used to store the items

    count = get_semaphore(generate_key(1));
    // This is the semaphore that will be used to keep track of the number of items in the buffer

    empty = get_semaphore(generate_key(2));
    // This is the semaphore that will be used to keep track of the number of empty spaces in the buffer

    printf("Semaphore %d is count semaphore\n", count);
    printf("Semaphore %d is empty semaphore\n", empty);

    int c_pointer = 0;
    while (true)
    {
        down(count);

        consume_item(&c_pointer, buffer);

        up(empty);

        nanosleep(&(struct timespec){0, period}, NULL);
    }

    return 0;
}