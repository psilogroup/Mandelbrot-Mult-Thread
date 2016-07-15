#include "ThreadPool.h"
#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include <stdio.h>
#define MAX_THREADS 64

//Pointer to Threads
SDL_Thread *threads[MAX_THREADS];

//Mutex for trhead safe sync
SDL_mutex *quequeMutex;
SDL_mutex *workerMutex;
SDL_mutex *mutexBarrier;

//Signals
SDL_cond *condHaveWork;
SDL_cond *condBarrier;


extern ThreadPool poolThread;

//This main function, while thread pool alive (detroy not called)
//this function monitor task queque, to process it

void *worker(void *param)
{
    while (1)
    {


        //Try to get Task
        Task *task = NULL;
        SDL_LockMutex(quequeMutex);
        //have task in queque
        int num = poolThread.workQueque.size();
        if (num > 0)
        {

            task = poolThread.workQueque.front();
            poolThread.workQueque.pop();
        }
        else
        {

        }
        SDL_UnlockMutex(quequeMutex);

        //if have task, run it
        if (task != NULL)
        {
            task->DoWork();
        }
         //if no have job thread in IDLE
        if (num == 0)
        {

            SDL_LockMutex(workerMutex);
            SDL_CondWait(condHaveWork,workerMutex);
            SDL_UnlockMutex(workerMutex);
        }
    }
}
ThreadPool::ThreadPool()
{
    //ctor

}

void ThreadPool::init(int _numThreads)
{
    //setup mutex
    quequeMutex = SDL_CreateMutex();
    workerMutex = SDL_CreateMutex();
    condHaveWork = SDL_CreateCond();
    mutexBarrier = SDL_CreateMutex();
    condBarrier = SDL_CreateCond();

    //setup number of threads
    numThreads = _numThreads;

    //initialize all threads
    for(int i = 0; i < numThreads; i++)
    {
        char threadName[128];
        sprintf(threadName,"Thread %d",i);
        //Create thread
        threads[i] = SDL_CreateThread(worker,threadName,&poolThread);
        if (NULL == threads[i])
        {
            printf ("Falha ao Criar thread \n");
        }

    }
}

//Destroy the thread pool
void ThreadPool::destroy()
{
    for (int i = 0; i < numThreads; i++)
    {
        //Kill threads
        if (threads[i] != NULL)
            SDL_DetachThread(threads[i]);
    }

    //Destroy Mutex and Signals
    SDL_DestroyMutex(quequeMutex);
    SDL_DestroyMutex(workerMutex);
    SDL_DestroyMutex(mutexBarrier);
    SDL_DestroyCond(condBarrier);
    SDL_DestroyCond(condHaveWork);
}


//Enqueque work to pool
void ThreadPool::addWork(Task *task)
{
    //Enqueque work
    SDL_LockMutex(quequeMutex);
    workQueque.push(task);
    int size = workQueque.size();
    SDL_UnlockMutex(quequeMutex);

    //Emit signal of new work
    SDL_LockMutex(workerMutex);
    SDL_CondSignal(condHaveWork);
    SDL_UnlockMutex(workerMutex);

}

ThreadPool::~ThreadPool()
{

}
