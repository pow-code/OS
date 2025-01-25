#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "queue.h"

const int PATH_MAXIMUM = 4096;
const int CODE_FATAL = -1001;
const int CODE_SUCCESS = 0;
const int CODE_ERROR = 1;

pthread_mutex_t dir_lock = PTHREAD_MUTEX_INITIALIZER;
char first_dst_path[PATH_MAX];

void dir_close(void *dir) {
    DIR *directory = (DIR *)dir;
    int err = closedir(directory);
    if (err != EXIT_SUCCESS) {
        fprintf(stderr, "<ERROR>: closedir(dir) failed: %s\n", strerror(errno));
    }
}

void file_close(void *fd) {
    intptr_t file = (intptr_t)fd;
    int err = close((int)file);
    if (err != EXIT_SUCCESS) {
        fprintf(stderr, "<ERROR>: close(file) failed: %s\n", strerror(errno));
    }
}

void *copy_file(const char *src_path, const char *dst_path) {
    int src_fd, dst_fd, err;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    struct stat statbuf;

    err = stat(src_path, &statbuf);
    if (err != EXIT_SUCCESS) {
        fprintf(stderr, "<ERROR>: stat(src_path) failed: %s\n", strerror(errno));
    }

    mode_t dst_mode = statbuf.st_mode & 0777;

    src_fd = open(src_path, O_RDONLY);

    while (src_fd == -1 && errno == EMFILE) {
        sleep(3);
        src_fd = open(src_path, O_RDONLY);
    }
    if (src_fd < 0) {
        fprintf(stderr, "<ERROR>: open() failed: cannot open file in src_path\n");
        pthread_exit(NULL);
    }

    pthread_cleanup_push(file_close, (void *)(intptr_t)src_fd);

    dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, dst_mode);
    while (dst_fd == -1 && errno == EMFILE) {
        sleep(3);
        dst_fd = open(dst_path, O_RDONLY);
    }
    if (dst_fd < 0) {
        fprintf(stderr, "<ERROR>: open() failed: cannot open file in dst_path\n");
        pthread_exit(NULL);
    }

    pthread_cleanup_push(file_close, (void *)(intptr_t)dst_fd);

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        int total_written = 0;

        while (total_written < bytes_read) {
            bytes_written = write(dst_fd, buffer + total_written, bytes_read - total_written);

            if (bytes_written < 0) {
                fprintf(stderr, "<ERROR>: write() failed: %s\n", strerror(errno));
                pthread_exit(NULL);
            }

            total_written += bytes_written;
        }
    }

    if (bytes_read < 0) {
        fprintf(stderr, "<ERROR>: read() failed: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);

    return NULL;
}

void *copy_dir(void *arg) {
    TaskQueue *queue = (TaskQueue *)arg;
    int err;

    while (queue->head != NULL) {
        Task *task = pop_task(queue);

        if (!task) {
            break;
        }

        struct stat statbuf;
        if (lstat(task->src_path, &statbuf) == 0) {
            mode_t dst_mode = statbuf.st_mode & 0777;

            if (access(task->src_path, R_OK) != 0) {
                fprintf(stderr, "can't access src_path: %s\n", strerror(errno));
            }

            err = pthread_mutex_lock(&dir_lock);
            if (err != EXIT_SUCCESS) {
                fprintf(stderr, "<ERROR>: pthread_mutex_lock() failed: %s\n", strerror(errno));
            }

            if (S_ISDIR(statbuf.st_mode)) {
                mkdir(task->dst_path, dst_mode);

                DIR *dir = opendir(task->src_path);
                if (dir == NULL) {
                  fprintf(stderr, "<ERROR>: opendir() failed: %s\n", strerror(errno));
                }

                if (dir) {
                    struct dirent *entry;
                    struct dirent entry_buffer;
                    int err;

                    while ((err = readdir_r(dir, &entry_buffer, &entry)) == 0 && entry != NULL) {
                        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                            continue;
                        }

                        char new_src_path[PATH_MAX];
                        char new_dst_path[PATH_MAX];
                        snprintf(new_src_path, PATH_MAX, "%s/%s", task->src_path, entry->d_name);
                        snprintf(new_dst_path, PATH_MAX, "%s/%s", task->dst_path, entry->d_name);

                        if (strcmp(first_dst_path, new_src_path) == 0) {
                            continue;
                        }

                        push_task(queue, new_src_path, new_dst_path);
                    }

                    if (err != 0) {
                        fprintf(stderr, "<ERROR>: readdir_r() failed: %s\n", strerror(err));
                    }

                    closedir(dir);
                }
            } else if (S_ISREG(statbuf.st_mode)) {
                copy_file(task->src_path, task->dst_path);
            }
            err = pthread_mutex_unlock(&dir_lock);
            if (err != EXIT_SUCCESS) {
                fprintf(stderr, "<ERROR>: pthread_mutex_unlock() failed: %s\n", strerror(errno));
            }
        }
        free_task(task);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int err;
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <source_path> <destination_path> <count_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];

    strncpy(first_dst_path, dst_path, strlen(dst_path));

    TaskQueue queue;
    init_task_queue(&queue);

    struct stat st;
    if (stat(dst_path, &st) != 0) {
        if (mkdir(dst_path, 0755) != 0) {
            fprintf(stderr, "<ERROR>: mkdir() failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    push_task(&queue, src_path, dst_path);

    const int NUM_THREADS = atoi(argv[3]);
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        err = pthread_create(&threads[i], NULL, copy_dir, &queue);
        if (err != EXIT_SUCCESS) {
            fprintf(stderr, "<ERROR>: pthread_create() failed: %s\n", strerror(errno));
            return -1;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        err = pthread_join(threads[i], NULL);
        if (err != EXIT_SUCCESS) {
            fprintf(stderr, "<ERROR>: pthread_join() failed: %s\n", strerror(errno));
            return -1;
        }
    }

    return EXIT_SUCCESS;
}
