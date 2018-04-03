#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
int main() {
    const char filename[] = "test.txt";
    int read_fd, write_fd;
    write_fd = open(filename, O_WRONLY | O_CREAT /*| O_EXLOCK*/, S_IWUSR | S_IRUSR); /* TODO: Use chroot for security */
    struct flock lock = {};
    memset (&lock, 0, sizeof(lock));

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(write_fd, F_SETLK, &lock) == -1) {
        printf("wtf\n");
        if (errno == EACCES || errno == EAGAIN) {
            printf("1\n");
            /* lock failed */
        } else {
            printf("2\n");
        }
    }

    memset (&lock, 0, sizeof(lock));
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_type = F_WRLCK;

    if (fcntl(write_fd, F_GETLK, &lock) == -1) {
        printf("get lock failed\n");
    }
    assert(lock.l_type == F_UNLCK);
    assert(lock.l_type == F_WRLCK);

    memset (&lock, 0, sizeof(lock));

    read_fd = open(filename, O_RDONLY);
    if (fcntl(read_fd, F_GETLK, &lock) == -1) {
        printf("get lock failed\n");
    }
    assert(lock.l_type == F_WRLCK);

    return 0;
}