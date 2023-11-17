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
#include <poll.h>

#include <linux/io_uring.h>

#define io_uring_smp_store_release(p, v)            \
    atomic_store_explicit((_Atomic typeof(*(p)) *)(p), (v), \
                  memory_order_release)

#define io_uring_smp_load_acquire(p)                \
    atomic_load_explicit((_Atomic typeof(*(p)) *)(p),   \
                 memory_order_acquire)


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

char *message = "Usage: io_uring_test <enclave_id> "
                "[-a IORING_SETUP_SQ_AFF] "
                "[-s size OPTIONAL RING SIZE]\n";

void
get_arguments(int argc, char *argv[], int *enclave_id, int *affinity_flag,
            int *ring_size)
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
    int enclave_id;
    int affinity_flag = 0;
    int ring_size = 16;
    char cmd_str[64];
    struct io_uring_params p = {0};


    get_arguments(argc, argv, &enclave_id, &affinity_flag, &ring_size);

    int cmd_pipe_fds[2];
    if(pipe(cmd_pipe_fds))
    {
        perror("control pipe failed");
        return EXIT_FAILURE;
    }


    //TODO iouring
    (void)fcntl(cmd_pipe_fds[0], F_SETFD,
            fcntl(cmd_pipe_fds[0], F_GETFD) | O_CLOEXEC);
    (void)fcntl(cmd_pipe_fds[1], F_SETFD,
            fcntl(cmd_pipe_fds[1], F_GETFD) | O_CLOEXEC);

    p.resv[0] = enclave_id; /* enclave id */
    p.resv[1] = cmd_pipe_fds[1]; /* Give enclave write end of exit pipe */
    p.sq_thread_idle = 120000; /* 2 minutes timeout */
    p.flags = IORING_SETUP_ENCLAVE | IORING_SETUP_SQPOLL;

    if(affinity_flag)
        p.flags = p.flags | IORING_SETUP_SQ_AFF;

    fprintf(stderr, "%s (%i): starting enclave id=%u...\n",
            argv[0], getpid(), p.resv[0]);
    fprintf(stderr, "%s (%i): cmd_pipe_fds[]=%i, %i\n", argv[0], getpid(),
            cmd_pipe_fds[0], cmd_pipe_fds[1]);

    long ring_fd = io_uring_setup(ring_size, &p);

    io_uring_enter(ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);

    while(1)
    {
        memset(cmd_str, '\0', sizeof(cmd_str));
        ssize_t cmd_str_size = read(cmd_pipe_fds[0], cmd_str,
            sizeof(cmd_str) - 1);

        if(cmd_str_size > 0)
        {
            const char exit_cmd[] = "exit: ";
            const char wake_cmd[] = "wake";

            if(strncmp(exit_cmd, cmd_str, sizeof(exit_cmd)-1) == 0)
            {
                /* read exit code from pipe */
                return atoi(cmd_str + sizeof(exit_cmd)-1);
            }
            else if(strncmp(wake_cmd, cmd_str, sizeof(wake_cmd)) == 0)
            {
                io_uring_enter(ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);
            }
            else
            {
                fprintf(stderr, "unknown command \"%s\"\n", cmd_str);
                return EXIT_FAILURE;
            }
        }
        else
        {
            perror("exit read failed");
            return EXIT_FAILURE;
        }
    }




}
