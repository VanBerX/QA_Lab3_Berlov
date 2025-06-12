#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "xrandom.h"


static int read_random(void *buf, int len)
{
    int fd, rc, cnt;
    fd = open("/dev/urandom", O_RDONLY);
    if(fd == -1)
        return 0;
    cnt = 0;
    while(cnt < len) {
        rc = read(fd, ((char*)buf) + cnt, len - cnt);
        if(rc < 1) {
                /* -1 is error, but, well, EOF on /dev/urandom is even worse */
            close(fd);
            return 0;
        }
        cnt += rc;
    }
    close(fd);
    return 1;
}

void randomize()
{
    unsigned int seed;
    int ok;
    ok = read_random(&seed, sizeof(seed));
    if(!ok) {
        seed = time(0) ^ (getpid() << 8);
            /* definitely worse, but better than nothing */
    }
    srand(seed);
}

void fill_random(void *buf, int len)
{
    int i;
    for(i = 0; i < len; i++)
        ((unsigned char*)buf)[i] = (rand() >> 7) & 0xff;
}
