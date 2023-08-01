#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

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


/*
* Read from completion queue.
* In this function, we read completion events from the completion queue.
* We dequeue the CQE, update and head and return the result of the operation.
* */
int read_from_cq() {
    struct io_uring_cqe *cqe;
    unsigned head;

    /* Read barrier */
    head = io_uring_smp_load_acquire(cring_head);


    /*
    * Remember, this is a ring buffer. If head == tail, it means that the
    * buffer is empty.
    * */
    if (head == *cring_tail)
        return -1;

    /* Get the entry */
    cqe = &cqes[head & (*cring_mask)];

    if (cqe->res < 0)
        fprintf(stderr, "Error: %s\n", strerror(abs(cqe->res)));

    head++;


    /* Write barrier so that update to the head are made visible */
    io_uring_smp_store_release(cring_head, head);

    return cqe->res;
}



/*
* Submit a read or a write request to the submission queue.
*
* */
int submit_to_sq(int fd, int op) {
    unsigned index, tail;

    /* Add our submission queue entry to the tail of the SQE ring buffer */
    tail = *sring_tail;

    index = tail & *sring_mask;

    struct io_uring_sqe *sqe = &sqes[index];

    /* Fill in the parameters required for the read or write operation */
    sqe->opcode = op;


    sqe->fd = fd;
    sqe->addr = (unsigned long) buff;


    if (op == IORING_OP_READ) {
        memset(buff, 0, sizeof(buff));
        sqe->len = BLOCK_SZ;
    }
    else {
        sqe->len = strlen(buff);
    }

    sqe->off = offset;

    sring_array[index] = index;

    tail++;


    /* Update the tail */
    io_uring_smp_store_release(sring_tail, tail);


    /*
    * Tell the kernel we have submitted events with the io_uring_enter()
    * system call. We also pass in the IOURING_ENTER_GETEVENTS flag which
    * causes the io_uring_enter() call to wait until min_complete
    * (the 3rd param) events complete.
    * */
    int ret =  io_uring_enter(ring_fd, 1,1,
                              IORING_ENTER_GETEVENTS);

    if(ret < 0) {
        perror("io_uring_enter");
        return -1;
    }

    return ret;
}




int main(int argc, char *argv[]) {
    int res;


    /* Setup io_uring for use */
    if(app_setup_uring()) {
        fprintf(stderr, "Unable to setup uring!\n");
        return 1;
    }


    /*
    * A while loop that reads from stdin and writes to stdout.
    * Breaks on EOF.
    */
    while (1) {
        /* Initiate read from stdin and wait for it to complete */
        submit_to_sq(STDIN_FILENO, IORING_OP_READ);

        /* Read completion queue entry */
        res = read_from_cq();

        if (res > 0) {

            /* Read successful. Write to stdout. */
            submit_to_sq(STDOUT_FILENO, IORING_OP_WRITE);

            read_from_cq();

        } else if (res == 0) {
            /* reached EOF */
            break;
        }
        else if (res < 0) {
            /* Error reading file */
            fprintf(stderr, "Error: %s\n", strerror(abs(res)));
            break;
        }

        offset += res;
    }

    return 0;
}





int main(int argc, char *argv[])
{
    struct io_uring_params p = {0};

    p.flags = (1U << 14) | IORING_SETUP_SQPOLL;

    printf("yeah yeah...\n");
    io_uring_setup(16, &p);
    printf("setup done!!!\n");
    return 0;
}
