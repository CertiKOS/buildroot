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



int main(int argc, char *argv[])
{
    struct io_uring_params p = {0};

    p.flags = IORING_SETUP_ENCLAVE | IORING_SETUP_SQPOLL;
    p.sq_thread_idle = 120000; /* 2 minutes timeout */
    p.resv[0] = 9; /* enclave id */

    printf("io_uring proxy: io_uring_setup for enclave id=%u...\n", p.resv[0]);

    long ret = io_uring_setup(16, &p);

    void* rings = mmap(0, p.sq_off.array + p.sq_entries * sizeof(uint32_t),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
        ret, IORING_OFF_SQ_RING);
    void* sqes = mmap(0, p.sq_entries * sizeof(struct io_uring_sqe),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
        ret, IORING_OFF_SQES);

    (void)sqes;

    void * sq_ptr = rings;
    //void * cq_ptr = rings;
    //uint32_t *sring_head = sq_ptr + p.sq_off.tail;
    //uint32_t *sring_tail = sq_ptr + p.sq_off.tail;
    uint32_t *sring_flags = sq_ptr + p.sq_off.flags;

    //uint32_t *cring_head = cq_ptr + p.cq_off.head;
    //uint32_t *cring_tail = cq_ptr + p.cq_off.tail;
    //uint32_t *sring_mask = sq_ptr + p.sq_off.mask;
    //uint32_t *sring_array = sq_ptr + p.sq_off.array;

    while(1) {
        if(io_uring_smp_load_acquire(sring_flags) & IORING_SQ_NEED_WAKEUP)
        {
            printf("io_uring proxy: waking up sleeping sq thread\n");
            io_uring_enter(ret, 0, 0, IORING_ENTER_SQ_WAKEUP);
        }
        sleep(1);
    };
    return 0;
}
