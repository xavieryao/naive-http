#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/file.h>
#include <string.h>
int main() {
    const char filename[] = "test.txt";
    int read_fd, write_fd;
    write_fd = open(filename, O_WRONLY /*| O_EXLOCK*/); /* TODO: Use chroot for security */
    read_fd = open(filename, O_RDONLY);

    int rc = 0;
    rc = flock(write_fd, LOCK_EX);
    printf("lock write %d\n", rc);

    rc = flock(read_fd, LOCK_SH);
    printf("lock write %d\n", rc);
    return 0;
}