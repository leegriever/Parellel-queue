#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>
#include "queue.h"

// my implementation uses 2 linked lists
// one represents the items Queue and the other represents the waiting threads

// structs

typedef struct queueNode {
    void *item;
    struct queueNode *next;
} queueNode;

typedef struct cvNode {
    cnd_t cv;
    void *item;
    struct cvNode *next;
} cvNode;

typedef struct Queue {
    queueNode *head;
    queueNode *tail;
    atomic_size_t size;
    atomic_size_t visited;
} Queue;

typedef struct conditionVariablesQueue {
    cvNode *head;
    cvNode *tail;
    atomic_size_t waiting;
} conditionVariablesQueue;

// global variables

mtx_t lock;
static Queue queue;
static conditionVariablesQueue cvQueue;

// auxilary function to handle both dequeue and tryDequeue
void* handleDequeue();

// functions 

void initQueue(void) { 
    // items queue init   
    queue.head = NULL;
    queue.tail = NULL;
    queue.size = 0;
    queue.visited = 0;
    
    // cvQueue init 
    cvQueue.head = NULL;
    cvQueue.tail = NULL;
    cvQueue.waiting = 0;

    mtx_init(&lock, mtx_plain);
}

void destroyQueue(void){
    mtx_destroy(&lock);

    // destroy cvQueue    
    cvNode *currCvNode = cvQueue.head;
    while (currCvNode) {
        cvNode *tmp = currCvNode->next;
        cnd_destroy(&currCvNode->cv);
        free(currCvNode);
        currCvNode = tmp;
    }
    free(currCvNode);

    // destroy queue
    queueNode *currNode = queue.head;
    while (currNode) {
        queueNode *tmp = currNode->next;
        free(currNode);
        currNode = tmp;
    }
    free(currNode);
}

void enqueue(void *item) {
    mtx_lock(&lock);
    // if there are threads sleeping, there is no need to add them to queue, just wake the latest thread
    if (cvQueue.head) {
        cvQueue.head->item = item;
        cnd_signal(&cvQueue.head->cv);
        cvQueue.head = cvQueue.head->next;
        mtx_unlock(&lock);
        return;
    }
    // there are no sleeping threads, then add item to queue
    queueNode *newElem = (queueNode *)malloc(sizeof(queueNode));
    newElem->item = item;
    newElem->next = NULL;
    // queue is empty
    if (queue.size == 0) {
        queue.head = newElem;
        queue.tail = newElem;
    }
    // queue is not empty
    else {
        queue.tail->next = newElem;
        queue.tail = newElem;
    }
    queue.size++;
    
    mtx_unlock(&lock);
}

void *dequeue(void) {
    mtx_lock(&lock);
    void* res = handleDequeue();
    mtx_unlock(&lock);
    return res;
}

void* handleDequeue(){
    cnd_t newcv;
    cnd_init(&newcv);
    cvNode *newCvNode = (cvNode *)malloc(sizeof(cvNode));
    newCvNode->cv = newcv;
    newCvNode->next = NULL;
    newCvNode->item = NULL;
    
    cvQueue.waiting++;
    // if the queue is not empty -> we don't have sleeping threads and we continue
    // else, we have sleeping threads and when signaled item is also initiazlized 
    while (queue.size == 0 && newCvNode->item == NULL) {

        if (cvQueue.head == NULL){
        cvQueue.head = newCvNode;
        cvQueue.tail = cvQueue.head;
        }
        else{
            cvQueue.tail->next = newCvNode;
            cvQueue.tail = cvQueue.tail->next;
        }
        cnd_wait(&newCvNode->cv, &lock);
    }

    void* res = newCvNode->item;
    cvQueue.waiting--;
    // this thread didn't wait ever
    if (res == NULL){
        queueNode *tmp = queue.head;
        queue.head = queue.head->next;
        tmp->next = NULL;
        res = tmp->item;
        free(tmp);
        queue.size--;
    }
    queue.visited++;
    free(newCvNode);
    return res;
}

bool tryDequeue(void** item){
    mtx_lock(&lock);
    if (queue.size != 0){
        void* res = handleDequeue();
        *item = res;
        mtx_unlock(&lock);
        return true;

    }
    mtx_unlock(&lock);
    return false;
}

size_t size(void) {
    return queue.size;
}

size_t waiting(void) {
    return cvQueue.waiting;
}

size_t visited(void) {
    return queue.visited;
}

