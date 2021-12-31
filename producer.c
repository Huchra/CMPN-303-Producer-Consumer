#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/signal.h>
#include <stdbool.h>
#include <time.h>

#define N 10
int shm_id, count, empty, mutex;
int sequence = -1;

int produce_item()
{
    // sequence = (sequence + 1) % N;
    sequence++;
    printf("Producer: Producing item %d\n", sequence);
    return sequence;
}

typedef union semaphore_t
{
    int val;               /* Value for SETVAL */
    struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO */
    void *__pad;
} semaphore_t;

int create_semaphore(key_t sem_key, int initial_value)
{
    semaphore_t sem;

    int sem_id = semget(sem_key, 1, IPC_CREAT | 0666);

    if (sem_id == -1)
    {
        perror("Error creating semaphore");
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

void clear_resources()
{
    if (shmctl(shm_id, IPC_RMID, NULL) == -1)
    {
        perror("Error clearing shared memory");
        exit(-1);
    }

    if (semctl(mutex, 0, IPC_RMID) == -1)
    {
        perror("Error clearing mutex semaphore");
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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Incorrect usage, to run use: %s <production rate>\n", argv[0]);
        exit(-1);
    }

    int period = 1000 / atoi(argv[1]);
    // This is the number of milliseconds between each production

    signal(SIGINT, clear_resources);

    shm_id = shmget(generate_key(0), N * sizeof(int), IPC_CREAT | 0666);
    // This is the shared memory segment that will be used to store the items

    count = create_semaphore(generate_key(1), 0);
    // This is the semaphore that will be used to keep track of the number of items in the buffer

    empty = create_semaphore(generate_key(2), N);
    // This is the semaphore that will be used to keep track of the number of empty spaces in the buffer

    mutex = create_semaphore(generate_key(3), 2);
    // This is the semaphore that will be used to control access to the buffer

    printf("Semaphore %d is count semaphore\n", count);
    printf("Semaphore %d is empty semaphore\n", empty);
    printf("Semaphore %d is mutex semaphore\n", mutex);

    // last production time
    time_t last_time = time(NULL);
    period = 1;

    while (true)
    {
        time_t current_time = time(NULL);
        if (current_time - last_time < period)
            continue;

        last_time = current_time;

        down(empty);
        down(mutex);

        int item = produce_item();
        int *buffer = shmat(shm_id, NULL, 0);

        int index = semctl(count, 0, GETVAL);
        buffer[index] = item;

        // print out buffer
        for (int i = 0; i < N; i++)
            printf("%d ", buffer[i]);
        printf("\n");

        up(mutex);
        up(count);
    }

    return 0;
}