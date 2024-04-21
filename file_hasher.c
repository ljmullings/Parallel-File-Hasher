#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define BSIZE 4096 //block size as prescribed by documentation 

typedef struct ThreadData
{
    uint64_t start_block;
    uint64_t num_blocks;
    int thread_id;
    int num_threads;
    uint8_t *file_data;
    size_t file_size;
} ThreadData;


void usage(char *s)
{
    fprintf(stderr, "Usage: %s filename num_threads\n", s);
    exit(EXIT_FAILURE);
}


uint32_t jenkins_one_at_a_time_hash(const uint8_t *key, size_t length)
{
    uint32_t hash = 0;
    for (size_t i = 0; i < length; i++)
    {
        hash += key[i];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

double GetTime(void) //implementation of GetTime T_T this one hurt 
{
    long            ms; // milliseconds
    time_t          s;  // seconds
    struct timespec spec;

    clock_gettime(0, &spec);

    s  = spec.tv_sec;
    ms = (int)(spec.tv_nsec / 1.0e6); // convert nanoseconds to milliseconds
    if (ms > 999) {
        s++;
        ms = 0;
    }

    return (double)s + (double)ms/1000;
}

//thread function!!! 
void *thread_func(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    //recursion: create/join threads for bitree struct 
    ThreadData left_child_data = *data;
    int left_child_id = data->thread_id * 2 + 1;
    pthread_t left_child;
    if (left_child_id < data->num_threads)
    {
        left_child_data.thread_id = left_child_id;
        left_child_data.start_block = left_child_id * data->num_blocks;
        int error = pthread_create(&left_child, NULL, thread_func, &left_child_data);
        if (error) {
            perror("Failed to create thread");
            pthread_exit(NULL);
            return NULL;
        }
    }
    else
    {
        left_child_id = -1;
    } 

    ThreadData right_child_data = *data;
    int right_child_id = data->thread_id * 2 + 2;
    pthread_t right_child;
    if (right_child_id < data->num_threads)
    {
        right_child_data.thread_id = right_child_id;
        right_child_data.start_block = right_child_id * data->num_blocks;
        int error = pthread_create(&right_child, NULL, thread_func, &right_child_data);
        if (error) {
            perror("Failed to create thread");
            pthread_exit(NULL);
            return NULL;
        }
    }
    else
    {
        right_child_id = -1; 
    }
    //calculating hash for current assigned block 
    off_t offset = data->start_block * BSIZE;
    uint64_t size = data->num_blocks * BSIZE;
    if ((offset + size) > data->file_size)
    {
        size = data->file_size - offset; //overflow note: offset needed for the overflow error
    }
    uint32_t hash = jenkins_one_at_a_time_hash(data->file_data + offset, size);

    //wait for completion and join
    uint32_t *left_hash = NULL;
    if (left_child_id > 0) {
        pthread_join(left_child, (void **)&left_hash);
    }
    uint32_t *right_hash = NULL;
    if (right_child_id > 0) {
        pthread_join(right_child, (void **)&right_hash);
    }

    if (left_hash || right_hash) 
    {
        // hashing combination from current -> child threads
        char hash_str[256];
        sprintf(hash_str, "%u", hash);
        if (left_hash) {
            sprintf(hash_str + strlen(hash_str), "%u", *left_hash);
            free(left_hash);
        }
        if (right_hash) {
            sprintf(hash_str + strlen(hash_str), "%u", *right_hash);
            free(right_hash);
        }

        hash = jenkins_one_at_a_time_hash((uint8_t *)hash_str, strlen(hash_str));
    }
    uint32_t *result = malloc(sizeof(uint32_t));
    if (result == NULL)
    {
        perror("Failed to allocate memory");
        pthread_exit(NULL);
    }
    *result = hash;
    pthread_exit(result);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint32_t num_threads = atoi(argv[2]);
    if (num_threads < 1)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    double start_time = GetTime(); //hashing timer starts
    struct stat st;
    if (stat(argv[1], &st) != 0)
    {
        perror("Failed to get file size");
        return EXIT_FAILURE;
    }
    size_t file_size = st.st_size;

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open file");
        return EXIT_FAILURE;
    }

    //mapping direct access for file by all threads
    void *file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);    
    if (file_data == MAP_FAILED)
    {
        perror("Failed to map file");
        close(fd);
        return EXIT_FAILURE;
    }
    //root thread data init
    pthread_t thread;
    ThreadData thread_data;
    uint64_t total_blocks = (file_size + BSIZE - 1) / BSIZE;
    thread_data.thread_id = 0;
    thread_data.num_threads = num_threads;
    thread_data.start_block = 0;
    thread_data.num_blocks = (total_blocks + num_threads - 1)/ num_threads;
    thread_data.file_data = (uint8_t *)file_data;
    thread_data.file_size = file_size;

    //starting root thread 
    int error = pthread_create(&thread, NULL, thread_func, &thread_data);
    int status = EXIT_SUCCESS;
    if (error) 
    {
        perror("Failed to create thread");
        status = EXIT_FAILURE;
    }
    else
    {
        // waiting for hashing completion and joining results
        uint32_t *final_result = NULL;
        pthread_join(thread, (void **)&final_result);
        if (final_result == NULL)
        {
            perror("Failed to get hash from first thread");
            status = EXIT_FAILURE;
        } 
        else
        {
            double end_time = GetTime();
            printf("Final hash value = %u\n", *final_result);
            printf("time taken = %f\n", end_time - start_time); 
            free(final_result);
        }
    }

    munmap(file_data, file_size);
    close(fd);

    return status;
}
