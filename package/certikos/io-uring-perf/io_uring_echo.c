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
#define READ_MAX_SIZE   (1024 * 1024)

int io_uring_setup(unsigned entries, struct io_uring_params *p)
{
    return (int) syscall(__NR_io_uring_setup, entries, p);
}

int io_uring_enter(int ring_fd, unsigned int to_submit,
                   unsigned int min_complete, unsigned int flags)
{
    return (int) syscall(__NR_io_uring_enter, ring_fd, to_submit,
        min_complete, flags, NULL, 0);
}

char *message = "Usage: io_uring_test <enclave_id> [-a IORING_SETUP_SQ_AFF] [-s size OPTIONAL RING SIZE]\n";

    void
    get_arguments(int argc, char *argv[], int *enclave_id, int *affinity_flag, int *ring_size)
{
    int opt;
    while ((opt = getopt(argc, argv, "as:")) != -1) {
        switch (opt) {
        case 'a':
            *affinity_flag = 1;
            break;
        case 's':
            *ring_size = atoi(optarg);
            break;
        default:
            printf(message);
            exit(-1);
        }
    }

    if (optind >= argc) {
        printf(message);
        exit(-1);
    }

    *enclave_id = atoi(argv[optind]);

}


int main(int argc, char *argv[])
{
    int ret;
    struct io_uring ring;
    struct io_uring_params p = {0};
    struct io_uring_sqe * sqe;
    struct io_uring_cqe * cqe;
    size_t min_entries = 16;
    int affinity = -1;

    /* scenario variables */
    static char block[1024*1024];
    size_t read_size = 0;


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

    io_uring_prep_read(sqe, STDIN_FILENO, block, READ_MAX_SIZE, 0);
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

                    io_uring_prep_write(sqe, STDOUT_FILENO, block, read_size, 0);
                    io_uring_sqe_set_data(sqe, (void*)WRITE_COOKIE);

                    ret = io_uring_submit(&ring);
                    if (ret < 1)
                    {
                        fprintf(stderr, "io_uring_submit read (%zu) error %i\n", read_size, ret);
                        return 1;
                    }

                }
                else if(cqe->res < 0)
                {
                    fprintf(stderr, "read: %s\n", strerror(-cqe->res));
                    return cqe->res;
                }
                else
                {
                    return 0;
                }
                break;


            case WRITE_COOKIE:
                if(cqe->res == read_size)
                {
                    sqe = io_uring_get_sqe(&ring);
                    if(!sqe)
                    {
                        fprintf(stderr, "io_uring_get_sqe error\n");
                        return 1;
                    }

                    io_uring_prep_read(sqe, STDIN_FILENO, block, read_size, 0);
                    io_uring_sqe_set_data(sqe, (void*)READ_COOKIE);

                    ret = io_uring_submit(&ring);
                    if (ret < 1)
                    {
                        fprintf(stderr, "io_uring_submit write error %i\n", ret);
                        return 1;
                    }
                }
                else
                {
                    fprintf(stderr, "write failed %i\n", cqe->res);
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
