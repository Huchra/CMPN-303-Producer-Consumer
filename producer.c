#include <stdlib.h>
#include <stdio.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/signal.h>
#include <stdbool.h>
#include <time.h>

int buffer_size, shm_id, count, empty, sequence = -1;

typedef union semaphore_t
{
    int val;               /* Value for SETVAL */
    struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO */
    void *__pad;
} semaphore_t;

void clear_resources()
{
    if (shmctl(shm_id, IPC_RMID, NULL) == -1)
    {
        perror("Error clearing shared memory");
        exit(-1);
    }

    if (semctl(empty, 0, IPC_RMID) == -1)
    {
        perror("Error clearing empty semaphore");
        exit(-1);
    }

    if (semctl(count, 0, IPC_RMID) == -1)
    {
        perror("Error clearing count semaphore");
        exit(-1);
    }

    exit(0);
}

key_t generate_key(int seed)
{
    return ftok("KeyFile", seed);
}

void send_buffer_size()
{
    int msg_id = msgget(generate_key(-1), IPC_CREAT | 0666);
    if (msg_id == -1)
    {
        perror("Error getting message queue");
        exit(-1);
    }

    if (msgsnd(msg_id, &buffer_size, sizeof(int), !IPC_NOWAIT) == -1)
    {
        perror("Error sending buffer size");
        exit(-1);
    }

    printf("Buffer size %d sent\n", buffer_size);
}

int create_semaphore(key_t sem_key, int initial_value)
{
    semaphore_t sem;

    int sem_id = semget(sem_key, 1, IPC_CREAT | 0666);

    if (sem_id == -1)
    {
        perror("Error getting semaphore");
        exit(-1);
    }

    sem.val = initial_value;
    if (semctl(sem_id, 0, SETVAL, sem) == -1)
    {
        perror("Error setting initial value of semaphore");
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

int produce_item()
{
    // sequence = (sequence + 1) % N;
    sequence++;
    printf("Producer: Producing item %d\n", sequence);
    return sequence;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Incorrect usage, to run use: %s <production rate>, <buffer size>\n", argv[0]);
        exit(-1);
    }

    int period = (1e9 - 1) / atoi(argv[1]);
    // This is the number of nanoseconds between each production

    buffer_size = atoi(argv[2]);
    send_buffer_size(buffer_size);
    // Sending the buffer size to the consumer

    signal(SIGINT, clear_resources);

    shm_id = shmget(generate_key(0), buffer_size * sizeof(int), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Error getting shared memory");
        exit(-1);
    }

    int *buffer = shmat(shm_id, NULL, 0);
    // This is the shared memory segment that will be used to store the items

    count = create_semaphore(generate_key(1), 0);
    // This is the semaphore that will be used to keep track of the number of items in the buffer

    empty = create_semaphore(generate_key(2), buffer_size);
    // This is the semaphore that will be used to keep track of the number of empty spaces in the buffer

    printf("Semaphore %d is count semaphore\n", count);
    printf("Semaphore %d is empty semaphore\n", empty);

    // Initialize all buffer values to -1
    for (int i = 0; i < buffer_size; ++i)
        buffer[i] = -1;

    while (true)
    {
        down(empty);

        int item = produce_item();
        buffer[sequence % buffer_size] = item;

        // Print out buffer
        for (int i = 0; i < buffer_size; i++)
            printf("%d ", buffer[i]);
        printf("\n");

        up(count);

        nanosleep(&(struct timespec){0, period}, NULL);
    }
    return 0;
}