#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096
#define PATH_MAX 4096

typedef struct Task {
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    struct Task *next;
} Task;

typedef struct {
    Task *head;
    Task *tail;
    pthread_mutex_t lock;
} TaskQueue;

void init_task_queue(TaskQueue *queue) {
  	int err;

    queue->head = NULL;
    queue->tail = NULL;
    err = pthread_mutex_init(&queue->lock, NULL);
    if (err != EXIT_SUCCESS) {
      	fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(err));
    }

}

void push_task(TaskQueue *queue, const char *src_path, const char *dst_path) {
  	int err;

    Task *new_task = malloc(sizeof(Task));
    strncpy(new_task->src_path, src_path, PATH_MAX);
    strncpy(new_task->dst_path, dst_path, PATH_MAX);
    new_task->next = NULL;

    err = pthread_mutex_lock(&queue->lock);
    if (err != EXIT_SUCCESS) {
      fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(err));
    }

    if (queue->tail) {
        queue->tail->next = new_task;
        queue->tail = new_task;
    } else {
        queue->head = queue->tail = new_task;
    }

    err = pthread_mutex_unlock(&queue->lock);
    if (err != EXIT_SUCCESS) {
      	fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(err));
    }
}

Task *pop_task(TaskQueue *queue) {
  	int err;

    err = pthread_mutex_lock(&queue->lock);
	if (err != EXIT_SUCCESS) {
          fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(err));
	}

    Task *task = queue->head;
    queue->head = task->next;
    if (!queue->head) {
        queue->tail = NULL;
    }

    err = pthread_mutex_unlock(&queue->lock);
    if (err != EXIT_SUCCESS) {
      fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(err));
    }
    return task;
}

void free_task(Task *task) {
    free(task);
}



