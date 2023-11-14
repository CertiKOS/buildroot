#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <linux/fs.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdatomic.h>
#include <getopt.h>

#include <linux/io_uring.h>
#include <liburing.h>

#include <certikos/profile.h>

#define READ_COOKIE     (1)
#define WRITE_COOKIE    (2)

inline uint64_t pmccntr_el0()
{
    uint64_t val=0;
    asm volatile("mrs %0, pmccntr_el0" : "=r" (val));
    return val;
}


inline uint64_t cntvct_el0()
{
    uint64_t val=0;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}

#define tsc()       cntvct_el0()
#define cycles()    pmccntr_el0()

struct echo_block
{
    struct echo_block * next;
    char * data;
    size_t data_size;
    size_t read_size;
    size_t write_size;
};

int in_fd = STDIN_FILENO;

struct echo_block *
echo_block_factory(size_t data_size)
{
    struct echo_block *ret = calloc(sizeof(struct echo_block), 1);
    ret->data_size = data_size;
    ret->data = malloc(data_size);
    return ret;
}


void
submit_read(struct io_uring *ring, struct echo_block * block)
{
    struct io_uring_sqe * sqe = io_uring_get_sqe(ring);
    if(!sqe)
    {
        fprintf(stderr, "io_uring_get_sqe error\n");
        exit(1);
    }

    io_uring_prep_read(sqe, in_fd,
            block->data, block->data_size, -1);
    io_uring_sqe_set_data(sqe, (void*)READ_COOKIE);

    int ret = io_uring_submit(ring);
    if (ret < 1)
    {
        fprintf(stderr, "io_uring_submit error %i\n", ret);
        exit(ret);
    }
}

void
submit_write(struct io_uring *ring, struct echo_block *block)
{
    struct io_uring_sqe * sqe = io_uring_get_sqe(ring);
    if(!sqe)
    {
        fprintf(stderr, "io_uring_get_sqe error\n");
        exit(1);
    }

    io_uring_prep_write(sqe, STDOUT_FILENO,
        block->data+block->write_size, block->read_size-block->write_size, -1);
    io_uring_sqe_set_data(sqe, (void*)WRITE_COOKIE);

    int ret = io_uring_submit(ring);
    if (ret < 1)
    {
        fprintf(stderr, "io_uring_submit after write error %i\n", ret);
        exit(ret);
    }
}



char *message = "Usage: %s [-a SQ_POLLING_AFFINITY] [-s RING_SIZE] [-b BLOCK_SIZE] [file path]\n";

void
get_arguments(int argc,
              char *argv[],
              int *sq_affinity,
              uint32_t *min_ring_size,
              size_t *block_size)
{
    int opt;
    while ((opt = getopt(argc, argv, "a:s:b:")) != -1) {
        switch (opt) {
        case 'a':
            *sq_affinity = atoi(optarg);
            break;
        case 's':
            *min_ring_size = atoi(optarg);
            break;
        case 'b':
            *block_size = atoi(optarg);
        default:
            fprintf(stderr, message, argv[0]);
            exit(-1);
        }
    }

    if(optind == argc - 1)
    {
        in_fd = open(argv[optind], O_RDONLY);
        if(in_fd < 0)
        {
            perror("failed to open input file");
            exit(errno);
        }
    }
}



int main(int argc, char *argv[])
{
    int ret;
    struct io_uring ring;
    struct io_uring_params p = {0};
    struct io_uring_cqe * cqe;
    uint32_t min_entries = 16;
    int affinity = -1;
    size_t block_size = 1024 * 64;
    struct echo_block *block_head = NULL;
    struct echo_block *block_tail = NULL;

    get_arguments(argc, argv, &affinity, &min_entries, &block_size);

    volatile uint64_t start_time = time();

    p.flags = IORING_SETUP_SQPOLL;
    p.sq_thread_idle = 120000; /* 2 minutes timeout */

    if(affinity >= 0)
    {
        p.flags = p.flags | IORING_SETUP_SQ_AFF;
        p.sq_thread_cpu = affinity;
    }

    ret = io_uring_queue_init_params(min_entries, &ring, &p);
    if(ret < 0)
    {
        fprintf(stderr, "io_uring_queue_init_params: %s\n", strerror(-ret));
        return ret;
    }

    block_head = echo_block_factory(block_size);
    block_tail = block_head;

    submit_read(&ring, block_tail);


    while(1)
    {
        ret = io_uring_peek_cqe(&ring, &cqe);
        if(ret == 0)
        {
            switch((uint64_t)cqe->user_data)
            {
            case READ_COOKIE:
                if(cqe->res > 0)
                {
                    block_tail->read_size = cqe->res;

                    if(block_head == block_tail)
                    {
                        submit_write(&ring, block_head);
                    }

                    block_tail->next = echo_block_factory(block_size);
                    block_tail = block_tail->next;
                    submit_read(&ring, block_tail);
                }
                else if(cqe->res < 0)
                {
                    fprintf(stderr, "read failed: %s\n", strerror(-cqe->res));
                    return cqe->res;
                }
                else
                {
                    if(block_head == block_tail)
                    {
                        uint64_t volatile end_time = time();
                        fprintf(stderr, "time=%lu %s\n", end_time - start_time,
                                time_unit());

                        io_uring_queue_exit(&ring);
                        return 0;
                    }
                    block_tail->next = (void *)-1;
                }
                break;


            case WRITE_COOKIE:
                if(cqe->res >= 0)
                {
                    block_head->write_size += cqe->res;

                    if(block_head->write_size > block_head->read_size)
                    {
                        fprintf(stderr, "wrote too many bytes\n");
                        exit(1);
                    }

                    if(block_head->write_size == block_head->read_size)
                    {
                        struct echo_block *old = block_head;
                        block_head = block_head->next;

                        if(block_head->next == (void *)-1)
                        {
                            uint64_t volatile end_time = time();
                            fprintf(stderr, "time=%lu %s\n", end_time -
                                    start_time, time_unit());

                            io_uring_queue_exit(&ring);
                            return 0;
                        }

                        free(old->data);
                        free(old);
                    }

                    if(block_head->read_size > 0)
                    {
                        submit_write(&ring, block_head);
                    }
                }
                else
                {
                    fprintf(stderr, "write failed: %s\n", strerror(-cqe->res));
                    return cqe->res;
                }
                break;
            }

            io_uring_cqe_seen(&ring, cqe);
        }
    }

    io_uring_queue_exit(&ring);
    return 0;
}
