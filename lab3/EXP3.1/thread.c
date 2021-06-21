#define debug(__T) printf("--test%d--\n", __T);

#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static int mutex[3] = {1, 1, 1};
struct mypara
{
    int pthread;
    int start;
    int end;
};

void waiting(int num)
{
    while(mutex[num] <= 0)
        ;
    mutex[num]--;
    return;
}
void signaling(int num)
{
    mutex[num]++;
}
void* thread_exec(void* arg)
{
    printf("Child Thread: \n");
    int result = execl("/bin/echo", "echo", __FILE__, __func__, (char*)NULL);
    getchar();
    return NULL;
}
void* thread_function(void* arg) {
    struct mypara* tmp = (struct mypara*)arg;
    char name[16];
    int i;
    if(tmp->start >= 30)
        strncpy(name, "Children: ", 16);
    else
        strncpy(name, "Parent :", 16);
    if(tmp->pthread == 1)
    {
        waiting(0);
    }
    sleep(1);
    if(tmp->pthread == 2)
    {
        waiting(1);
        waiting(0);
    }
    sleep(1);
    if(tmp->pthread == 3)
    {
        waiting(2);
        waiting(1);
        waiting(0);
    }
    printf("Thread%d working...!\n", tmp->pthread);
    for (i = tmp->start; i < tmp->end; i++) {
        printf("%sNow the time is %d The mutex is %d and %d\n", name, i, mutex[0], mutex[1]);
        sleep(1);
    }
    if(tmp->pthread == 1)
    {
        signaling(0);
    }
    if(tmp->pthread == 2)
    {
        signaling(1);
        signaling(0);
    }
    if(tmp->pthread == 3)
    {
        signaling(2);
        signaling(1);
        signaling(0);
    }
    getchar();
    return NULL;
}
int main(void) {
    pid_t pid;
    pthread_t mythread1;
    pthread_t mythread2;
    pthread_t mythread3;
    struct mypara p1 = { 1, 0, 10 };
    struct mypara p2 = { 2, 10, 20 };
    struct mypara p3 = { 3, 20, 30 };
    struct mypara p4 = { 1, 30, 40 };
    struct mypara p5 = { 2, 40, 50 };
    struct mypara p6 = { 3, 50, 60 };
    int i = 0, j = 10, k = 20;
    pid = fork();
    if (pid < 0)
    {
        perror("fork failed!\n");
    }
    if (pid == 0)
    {
        pthread_t sonthread1;
        pthread_t sonthread2;
        pthread_t sonthread3;
        pthread_t echothread;
        printf("This is the child process.\n");
        if (pthread_create(&sonthread1, NULL, thread_function, &p4)) {
            printf("error creating thread1.\n");
            abort();
        }
        if (pthread_create(&sonthread2, NULL, thread_function, &p5)) {
            printf("error creating thread2.");
            abort();
        }
        if (pthread_create(&sonthread3, NULL, thread_function, &p6)) {
            printf("error creating thread3.");
            abort();
        }
        sleep(1);
        while(mutex[0] <= 0 || mutex[1] <= 0 || mutex[2] <= 0);
        if (pthread_create(&echothread, NULL, thread_exec, NULL)) {
            printf("error creating thread echo.\n");
            abort();
        }
        if (pthread_join(sonthread1, NULL)) {
            printf("error join thread.");
            abort();
        }
        //debug(1)
        if (pthread_join(sonthread2, NULL)) {
            printf("error join thread.");
            abort();
        }
        //debug(2)
        if (pthread_join(sonthread3, NULL)) {
            printf("error join thread.");
            abort();
        }
        //debug(3)
        sleep(1);
        while(mutex[0] <= 0 || mutex[1] <= 0 || mutex[2] <= 0);
        sleep(1);
        if (pthread_join(echothread, NULL)) {
            printf("error join thread.");
            abort();
        }
        getchar();
    }
    else
    {
        if (pthread_create(&mythread1, NULL, thread_function, &p1)) {
            printf("error creating thread1.");
            abort();
        }
        if (pthread_create(&mythread2, NULL, thread_function, &p2)) {
            printf("error creating thread2.");
            abort();
        }
        if (pthread_create(&mythread3, NULL, thread_function, &p3)) {
            printf("error creating thread3.");
            abort();
        }
        if (pthread_join(mythread1, NULL)) {
            printf("error join thread.");
            abort();
        }
        if (pthread_join(mythread2, NULL)) {
            printf("error join thread.");
            abort();
        }
        if (pthread_join(mythread3, NULL)) {
            printf("error join thread.");
            abort();
        }
    }
    printf("thread done! \n");
    exit(0);
}