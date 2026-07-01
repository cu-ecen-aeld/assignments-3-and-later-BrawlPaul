#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    openlog("assignment2", 0, LOG_USER);
    if (argc != 3)
    {
        syslog(LOG_ERR, "Incorrect Number of args specified");
        exit(1);
    }
    char *fileName = argv[1];
    char *fileStr = argv[2];
    int fd = open(fileName, O_CREAT | O_RDWR, 0644);
    if (fd < 0)
    {
        syslog(LOG_ERR, "Error opening file");
        exit(1);
    }    
    syslog(LOG_DEBUG, "Writing %s to %s", fileStr, fileName);
    write(fd, fileStr, strlen(fileStr));
    write(fd, "\n", 1);
    close(fd);
}