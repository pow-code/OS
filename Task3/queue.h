#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>
#include <stdbool.h>

// Размер буфера для копирования файлов
#define BUFFER_SIZE 4096
#define PATH_MAX 4096

// Структура задачи
typedef struct Task {
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    struct Task *next;
} Task;

// Структура очереди задач
typedef struct {
    Task *head;
    Task *tail;
    pthread_mutex_t lock;
} TaskQueue;

// Функции работы с очередью задач
void init_task_queue(TaskQueue *queue);
void push_task(TaskQueue *queue, const char *src_path, const char *dst_path);
Task *pop_task(TaskQueue *queue);
void finish_task_queue(TaskQueue *queue);
void free_task(Task *task);
void skip_task(TaskQueue *queue);

#endif // TASK_QUEUE_H
