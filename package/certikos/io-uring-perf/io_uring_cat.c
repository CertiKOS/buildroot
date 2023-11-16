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

int in_fd = STDIN_FILENO;

char *message =
    "Usage: %s [-a SQ_POLLING_AFFINITY] [-s RING_SIZE] [-b BLOCK_SIZE]\n";

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
    struct io_uring_sqe * sqe;
    struct io_uring_cqe * cqe;
    uint32_t min_entries = 16;
    int affinity = -1;
    size_t block_size = 1024 * 64;

    get_arguments(argc, argv, &affinity, &min_entries, &block_size);

    volatile uint64_t start_time = time();

    /* scenario variables */
    char *block = malloc(block_size);
    memset(block, 0, block_size); /* Trigger all page faults, warm cache */

    size_t read_size = 0;
    size_t write_size = 0;


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


    sqe = io_uring_get_sqe(&ring);
    if(!sqe)
    {
        fprintf(stderr, "io_uring_get_sqe error\n");
        return 1;
    }

    io_uring_prep_read(sqe, in_fd, block, block_size, 0);
    io_uring_sqe_set_data(sqe, (void*)READ_COOKIE);

    ret = io_uring_submit(&ring);
    if (ret != 1)
    {
        fprintf(stderr, "io_uring_submit error %i\n", ret);
        return 1;
    }

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
                    read_size = cqe->res;
                    sqe = io_uring_get_sqe(&ring);
                    if(!sqe)
                    {
                        fprintf(stderr, "io_uring_get_sqe error\n");
                        return 1;
                    }

                    io_uring_prep_write(sqe, STDOUT_FILENO, block, read_size, -1);
                    io_uring_sqe_set_data(sqe, (void*)WRITE_COOKIE);

                    ret = io_uring_submit(&ring);
                    if (ret < 1)
                    {
                        fprintf(stderr, "submit read error: %i\n", ret);
                        return 1;
                    }

                }
                else if(cqe->res < 0)
                {
                    fprintf(stderr, "read failed: %s\n", strerror(-cqe->res));
                    return cqe->res;
                }
                else
                {
                    uint64_t volatile end_time = time();
                    fprintf(stderr, "time=%lu %s\n", end_time - start_time,
                        time_unit());
                    io_uring_queue_exit(&ring);
                    return 0;
                }
                break;


            case WRITE_COOKIE:
                if(cqe->res >= 0)
                {
                    write_size += cqe->res;

                    sqe = io_uring_get_sqe(&ring);
                    if(!sqe)
                    {
                        fprintf(stderr, "io_uring_get_sqe error\n");
                        return 1;
                    }

                    if(write_size == read_size)
                    {
                        write_size = 0;
                        io_uring_prep_read(sqe, in_fd, block, block_size, -1);
                        io_uring_sqe_set_data(sqe, (void*)READ_COOKIE);
                    }
                    else
                    {
                        io_uring_prep_write(sqe, STDOUT_FILENO,
                                block+write_size, read_size-write_size, -1);
                        io_uring_sqe_set_data(sqe, (void*)WRITE_COOKIE);
                    }


                    ret = io_uring_submit(&ring);
                    if (ret < 1)
                    {
                        fprintf(stderr, "submit write error: %i\n", ret);
                        return 1;
                    }
                }
                else
                {
                    fprintf(stderr, "write failed: %s\n", strerror(-cqe->res));
                    return cqe->res;
                }
                break;
            default:
                fprintf(stderr, "Unknown cookie\n");
                return 1;
            }

            io_uring_cqe_seen(&ring, cqe);
        }

    }

    io_uring_queue_exit(&ring);
    return 0;
}
