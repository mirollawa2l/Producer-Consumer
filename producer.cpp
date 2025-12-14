// shared buffer ipc
// no more one producer write at the same time
// each produce type , price standard deviation mean
// mutex
// counting semaphore donot produce in full buffer , consume from empty buffer
#include <syscall.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <random>
#include <time.h>
#include <unistd.h>

using namespace std;
int N;
int producers = 20;
// commodity items
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
  int active_producers ; // for cleanup shared memory aand semaphoore
} memory_block;

// union manage and communicate with n semaphores
//  each semaphore has it's own value
union semun
{
  int value;
  struct semid_ds *buf; // this struct hold info about
                        // semaphore set struct is to large to pass it by value
  unsigned short *array;
};

void print(string name, string msg, int value, bool p)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  time_t now_sec = ts.tv_sec;
  struct tm *t = localtime(&now_sec);

  long ms = ts.tv_nsec / 1000000; // convert nanoseconds to milliseconds

  fprintf(stderr, "[%02d/%02d/%04d %02d:%02d:%02d.%03ld] %s: ",
          t->tm_mon + 1, t->tm_mday, t->tm_year + 1900,
          t->tm_hour, t->tm_min, t->tm_sec, ms, name.c_str());
  if (p)
    fprintf(stderr, "%s %d\n", msg.c_str(), value);
  else
    fprintf(stderr, "%s\n", msg.c_str());
}
void produce(item prodItem, memory_block *m, int semid, item *buffer)
{

  struct sembuf signal_mutex = {0, 1, 0}; // 0->mutex semaphore 0 ,  1 ->signal
  struct sembuf wait_mutex = {0, -1, 0};
  struct sembuf wait_empty = {1, -1, 0};
  struct sembuf signal_full = {2, 1, 0};
  default_random_engine generator;
  normal_distribution<double> distribution(prodItem.mean, prodItem.standardDev);
  while (true)
  {
    double price = 0; // to ensure generated price is postive
    do
    {
      price = distribution(generator);
    } while (price <= 0);

    print(prodItem.name, "generating a new value", price, true);
    prodItem.price = price;
    semop(semid, &wait_empty, 1);
    print(prodItem.name, "trying to get mutex on shared buffer", 0, false);
    semop(semid, &wait_mutex, 1); /* enter critical section*/
    print(prodItem.name, "placing  on shared buffer", price, true);
    prodItem.price = price;
    buffer[m->in] = prodItem;
    m->in = (m->in + 1) % N;
    m->curr_size++;
    fprintf(stderr, "DBG %s: in=%d out=%d curr_size=%d\n", prodItem.name, m->in, m->out, m->curr_size);

    semop(semid, &signal_mutex, 1);
    semop(semid, &signal_full, 1);
    print(prodItem.name, "sleeping for", prodItem.time, true);
    usleep(prodItem.time * 1000);
  }
}
void cleanup(int signum)
{
 key_t shm_key = ftok("file", 3);
  key_t sem_key = ftok("file", 4);

  int shmid = shmget(shm_key, 0, 0666);
  int semid = semget(sem_key, 0, 0666);

  if (shmid != -1 && semid != -1)
  {
    memory_block *shared = (memory_block *)shmat(shmid, NULL, 0);
    
    struct sembuf wait_mutex = {0, -1, 0};
    struct sembuf signal_mutex = {0, 1, 0};
    
    semop(semid, &wait_mutex, 1);
    shared->active_producers--;
    int remaining = shared->active_producers;
    semop(semid, &signal_mutex, 1);
    
    shmdt(shared);  // Detach from shared memory
        // edit the cleanup logic doesnot remove the shared memory only if last producer running
    if (remaining == 0)
    {
      fprintf(stderr, "Last producer exiting, cleaning up resources\n");
      shmctl(shmid, IPC_RMID, NULL);
      semctl(semid, 0, IPC_RMID, NULL);
    }
    else
    {
      fprintf(stderr, "Producer exiting, %d producers still running\n", remaining);
    }
  }

  exit(0);
}

int main(int argc, char *args[])
{
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup); // to clear shared memory and semaphore ctrl c
  signal(SIGTSTP, cleanup);
  if (argc != 6)
  {
    perror("Usage: ./producer name mean stddev time N");
    exit(1);
  }
  N = atoi(args[5]);
 
  // shared memory for ipc
  key_t sharedMem_key = ftok("file", 3); // generate unique ipc key
  // unique key for semaphore
  key_t key = ftok("file", 4);
  if (sharedMem_key == -1)
  {
    perror("ftok fail to generate unique key missing file path");
    exit(1);
  }
  if (key == -1)
  {
    perror("ftok fail to generate unique key missing file path");
    exit(1);
  }

  // allocate size

  int shmid = shmget(sharedMem_key, sizeof(memory_block) + sizeof(item) * N, 0666 | IPC_CREAT | IPC_EXCL); // shared memory creation
  bool first_producer = true;
  if (shmid == -1)
  {
    if (errno == EEXIST)
    {
      first_producer = false;
      shmid = shmget(sharedMem_key, sizeof(memory_block) + sizeof(item) * N, 0666);
      if (shmid == -1)
      {
        perror("shmget open failed");
        exit(1);
      }
    }
    else
    {
      perror("shmget create failed");
      exit(1);
    }
  }
  //  if(!first_producer )
  //    shmid = shmget(sharedMem_key , sizeof(memory_block)+sizeof(item) * N, 0666); // just attach the producer to shared memory

  memory_block *shared = (memory_block *)shmat(shmid, NULL, 0);
  item *buffer = (item *)((char *)shared + sizeof(memory_block));
  int semid = semget(key, 3, 0666 | IPC_CREAT | IPC_EXCL); // create semaphore set
   
                                             //                                                 // if two processes use the same key they access the samme semaphore set

  if (semid == -1)
  {
    if (errno == EEXIST)
    {
      semid = semget(key, 3, 0666);
      if (semid == -1)
      {
        perror("semget open failed");
        exit(1);
      }
      int actualN = shared->size ;
       if (actualN != N) {
        fprintf(stderr, "Warning: producer argv N=%d differs from shared size=%d. Using shared size.\n", N, actualN);
        N = actualN; // override local N to match shared memory
    }    
    struct sembuf wait_mutex = {0, -1, 0};
    struct sembuf signal_mutex = {0, 1, 0};
    
    semop(semid, &wait_mutex, 1);
    shared->active_producers++;
    fprintf(stderr, "[DEBUG] Producer joined. Active producers: %d\n", shared->active_producers);
    semop(semid, &signal_mutex, 1);
    }
    else
    {
      perror("semget create failed");
      exit(1);
    }
  }
  else
  {
    if (first_producer)
    {
      shared->in = 0;
      shared->out = 0;
      shared->curr_size = 0;
      shared->size = N;
      shared->active_producers =1;
      union semun sem_union;
      // to set value of 3 semaphores at the same time 1->mutex critical section free
      // N -> counting semaphor N free space in buffer // 0 => counting semaphore 0 waiting
      unsigned short arr[3] = {1, (unsigned short)N, 0};
      sem_union.array = arr;
      if (semctl(semid, 0, SETALL, sem_union) == -1)
      {
        perror("semctl fail");
        exit(1);
      }
      else
      {
        fprintf(stderr, "[DEBUG] Semaphore values: mutex=%d empty=%d full=%d\n",
                arr[0], arr[1], arr[2]);
      }
    }
   
  }
  item i;
  strcpy(i.name, args[1]);
  i.mean = atof(args[2]);
  i.standardDev = atof(args[3]);
  i.time = atoi(args[4]);
  i.size = atoi(args[5]);
  
  while (true)
  {
    produce(i, shared, semid, buffer);
  }
}
/*
./producer GOLD 4 0.2 700 4
./producer SILVER 3 0.3 900 4
./producer LEAD 5 0.5 800 4
  ./producer ZINC 9 0.85 850 4
  ./producer COTTON 10 0.6 750 4
./producer COPPER 7 0.4 600 4



*/