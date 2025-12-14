#include <iostream>
#include <string>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <iomanip>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define BLUE "\033[34m"
#define RESET "\033[0m"

using namespace std;

int N;

/*
struct sembuf {
    unsigned short sem_num;   // which semaphore in the set?
    short          sem_op;    // what operation? (-1, +1, 0)
    short          sem_flg;   // options, usually 0
};
*/

typedef struct
{
    char name[10];
    double mean;
    double standardDev;
    double price;
    int time;
    int size;
} item;

typedef struct
{
    int in;  // produced
    int out; // consumed
    int curr_size;
    int size;
    int active_producers; // for cleanup shared memory aand semaphoore

    // item buffer[1];
} memory_block;

typedef struct
{
    string name;
    double price;
    double avgPRice;
} Commodity;

typedef struct
{
    double history[5];
    int count;
    double current;
    double average;
    bool arrowUp;
} CommodityInfo;

/*
0-> mutex
1-> empty
2-> full
*/

// probern test/wait -> wait and decrement
struct sembuf P(int semIndex)
{
    return {(unsigned short)semIndex, -1, 0};
}

// verhogen increment -> signal and increment
struct sembuf V(int semIndex)
{
    return {(unsigned short)semIndex, 1, 0};
}

Commodity commodities[11];
CommodityInfo info[11];
map<string, int> commodityIndex;

void initialize()
{
    memset(commodities, 0, sizeof(commodities)); // fill all bytes with 0
    memset(info, 0, sizeof(info));               // fill all bytes with 0
    commodities[0].name = "ALUMINIUM";
    commodities[1].name = "COPPER";
    commodities[2].name = "COTTON";
    commodities[3].name = "CRUDEOIL";
    commodities[4].name = "GOLD";
    commodities[5].name = "LEAD";
    commodities[6].name = "MENTHAOIL";
    commodities[7].name = "NATURALGAS";
    commodities[8].name = "NICKEL";
    commodities[9].name = "SILVER";
    commodities[10].name = "ZINC";
    for (int i = 0; i < 11; i++)
    {
        commodityIndex[commodities[i].name] = i;
    }
}

int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        cerr << "Usage: ./consumer N\n";
        exit(1);
    }

    N = atoi(argv[1]);

    initialize();

    // shared memory for ipc
    key_t shm_key = ftok("file", 3); // generate unique ipc key
    if (shm_key == -1)
    {
        perror("ftok fail to generate shared memory unique key missing file path");
        exit(1);
    }
    key_t sem_key = ftok("file", 4);

    if (sem_key == -1)
    {
        perror("ftok fail to generate semaphore unique key missing file path");
        exit(1);
    }

    // connect to existing shared memory
    int shmid = shmget(shm_key, sizeof(memory_block) + sizeof(item) * N, 0666);
    if (shmid == -1)
        if (shmid == -1)
        {
            perror("shmget failed run a producer first");
            exit(1);
        }

    memory_block *shared = (memory_block *)shmat(shmid, NULL, 0);

    if (shared == (void *)-1)
    {
        perror("shmat failed");
        exit(1);
    }

    int actualN = shared->size;
    if (actualN <= 0)
    {
        // defensive fallback: if producer did not set capacity, use argv N
        actualN = N;
        fprintf(stderr, "Warning: shared->size is 0 or unset; falling back to argv N=%d\n", N);
    }
    if (actualN != N)
    {
        fprintf(stderr, "Warning: consumer argv N=%d differs from shared size=%d. Using shared size.\n", N, actualN);
        N = actualN; // override local N to match shared memory
    }

    item *buffer = (item *)((char *)shared + sizeof(memory_block));

    int semid = semget(sem_key, 3, 0666);
    if (semid == -1)
    {
        perror("semget failed");
        exit(1);
    }

    /*
    wait(full)
    wait(mutex)
    critical section
    signal(mutex)
    signal(empty)
    */
    while (true)
    {
        // Wait (P) on full semaphore
        struct sembuf op;
        op = P(2);
        if (semop(semid, &op, 1) == -1)
        {
            perror("semop P(full) (wait full) failed");
            break;
        }

        // Wait (P) on mutex semaphore
        op = P(0);
        if (semop(semid, &op, 1) == -1)
        {
            perror("semop P(mutex) (wait mutex) failed");
            break;
        }

        // Critical Section

        if (shared->out < 0 || shared->out >= N)
        {
            // sanity guard: if index is corrupted, clamp it
            fprintf(stderr, "Warning: shared->out=%d out of bounds (0..%d). Resetting to 0.\n", shared->out, N - 1);
            shared->out = 0;
        }
        item consumedItem = buffer[shared->out];
        shared->out = (shared->out + 1) % N;
        if (shared->curr_size > 0)
            shared->curr_size--;
        else
        {
            // defensive: curr_size shouldn't go negative
            shared->curr_size = 0;
        }
        fprintf(stderr, "DBG CONSUMER: in=%d out=%d curr_size=%d\n", shared->in, shared->out, shared->curr_size);
        // Signal (V) on mutex semaphore
        op = V(0);
        if (semop(semid, &op, 1) == -1)
        {
            perror("semop V(mutex) (signal wait) failed");
            break;
        }

        // Signal (V) on empty semaphore
        op = V(1);
        if (semop(semid, &op, 1) == -1)
        {
            perror("semop V(empty) (signal empty) failed");
            break;
        }

        // Process consumed item
        bool colored[11];
        for (int i = 0; i < 11; i++)
            colored[i] = false;
        string itemName(consumedItem.name);
        double price = consumedItem.price;
        cout << "Item name: " << itemName << endl;
        cout << itemName << " consumed value: " << price << endl;
        int index = commodityIndex[itemName];
        commodities[index].price = price;

        double prevAvg = info[index].average;

        info[index].history[info[index].count % 5] = price;
        info[index].current = price;
        double sum = 0.0;
        int histCount = min(info[index].count + 1, 5);
        for (int i = 0; i < histCount; i++)
        {
            sum += info[index].history[i];
        }

        info[index].average = sum / histCount;

        if (info[index].count > 0)
        {
            info[index].arrowUp = (info[index].average > prevAvg);
            colored[index] = true;
        }
        else
        {
            info[index].arrowUp = true;
            colored[index] = true;
        }

        info[index].count++;

        cout << "+--------------------------------------+\n";

        cout << left << "| " << setw(12) << "Commodity"
             << " | " << "  Price  "
             << " | " << " AvgPrice " <<"| "<< endl;
        cout << "+--------------------------------------+\n";

        for (int i = 0; i < 11; ++i)
        {
            string arrow = " ";
            if (info[i].count > 0)
                arrow = info[i].arrowUp ? "↑" : "↓";

            cout << left << "| " << setw(12) << commodities[i].name
                 <<left<< " | " << (colored[i] ? RED : BLUE)<<setw(8) << fixed << setprecision(2) << info[i].current << arrow << RESET
                 <<left<< " | " << (colored[i] ? RED : BLUE) <<setw(8)<< fixed << setprecision(2) << info[i].average << arrow
                  << RESET << " | " << endl;
        }
        cout << "+--------------------------------------+\n";

        usleep(150000); // 150 ms
    }

    // detach shared memory on exit (never reached in normal infinite loop)
    shmdt(shared);
}