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
#include <liburing.h>
#include <certikos/spawn.h>

int main(int argc, char *argv[], char *envp[])
{
    int ret;
    const char * env;

    /* params */
    char *enclave_name;
    int affinity_flag = 0;
    uint32_t min_entries = 16;
    int partition_id = 5;
    int period = 2;
    int max_budget = 1;
    int priority = 1;
    int quota_pages = -1;


    /* io_uring */
    struct io_uring ring;
    struct io_uring_params p = {0};
    struct io_uring_sqe * sqe;

    /* commands */
    char cmd_str[64];
    int cmd_pipe_fds[2];


    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s <enclave_name> [enclave args]...\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if((env = getenv("ENCLAVE_PARTITION")) != NULL)
    {
        partition_id = atoi(env);
    }

    if((env = getenv("ENCLAVE_PERIOD")) != NULL)
    {
        period = atoi(env);
    }

    if((env = getenv("ENCLAVE_MAX_BUDGET")) != NULL)
    {
        max_budget = atoi(env);
    }

    if((env = getenv("ENCLAVE_PRIORITY")) != NULL)
    {
        priority = atoi(env);
    }

    if((env = getenv("ENCLAVE_QUOTA_PAGES")) != NULL)
    {
        quota_pages = atoi(env);
    }


    enclave_name = argv[1];
    //TODO getenv affinity, min_entries, quota, RT-PARAMS

    if(pipe(cmd_pipe_fds))
    {
        perror("control pipe failed");
        return EXIT_FAILURE;
    }

    //TODO iouring (don't need to GETFD because CLOEXEC is the only option
    (void)fcntl(cmd_pipe_fds[0], F_SETFD, O_CLOEXEC);
    (void)fcntl(cmd_pipe_fds[1], F_SETFD, O_CLOEXEC);


    p.sq_thread_idle = 120000; /* 2 minutes timeout */
    p.flags = IORING_SETUP_SQPOLL;

    if(affinity_flag)
        p.flags = p.flags | IORING_SETUP_SQ_AFF;

    fprintf(stderr, "%s (%i): starting enclave \"%s\"...\n",
            argv[0], getpid(), enclave_name);

    ret = io_uring_queue_init_params(min_entries, &ring, &p);
    if(ret < 0)
    {
        fprintf(stderr, "io_uring_queue_init_params: %s\n", strerror(-ret));
        return EXIT_FAILURE;
    }

    struct sys_spawn_param_t spawn_param = {
        .bin_name = enclave_name,
        .argv = argv + 1,
        .envp = envp,
        .quota_pages = quota_pages,

        /* RT-PARAMS */
        .sched_policy = SYS_SCHED_POLICY_RT_FP,
        .partition_id = partition_id,
        .period = period,
        .max_budget = max_budget,
        .priority = priority,
    };

    p.resv[1] = cmd_pipe_fds[1]; /* Give enclave write end of exit pipe */

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_rw(IORING_OP_ENCLAVE_SPAWN, sqe, -1,
            &spawn_param, 0, (uintptr_t)&p);
    io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);
    io_uring_submit(&ring);

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
                io_uring_submit(&ring);
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
