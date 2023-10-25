#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>



uint64_t cntvct_el0()
{
    uint64_t val=0;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}

uint64_t cntfrq_el0()
{
    uint64_t val=0;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (val));
    return val;
}

int
main(int argc, const char *argv[])
{
    uint64_t counter = 0;
    uint64_t period = 1000*1000;
    uint64_t last_time = 0;
    uint64_t ticks_per_usec = cntfrq_el0() / (1000*1000);
    pid_t pid = getpid();

    if(argc > 2 || (argc ==  2 && (period = atoll(argv[1])) <= 0))
    {
        fprintf(stderr, "usage: periodic_ping [USEC_PERIOD]\n");
        return -1;
    }

    while(1)
    {
        uint64_t t = cntvct_el0();

        uint64_t delta = (t - last_time)/ticks_per_usec;
        double delta_err = 100.0*(delta - period)/(double)period;

        printf("%s (%u): %zu, delta=%zu usec, %0.3f error %s\n",
            argv[0], pid, counter++, delta, delta_err, delta_err > 1.0 ? "DEADLINE MISS" : "");


        last_time = cntvct_el0();
        if(usleep(period) != 0) return -1;
    }

    return 0;
}
