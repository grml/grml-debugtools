/*
 * (c) Michael Gebetsroither <michael.geb@gmx.at>
 * gpl version 2 or any later version
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


// DEBUG enabled
#ifndef NDEBUG
#define dperror(message) \
{ \
  fflush(stdout); \
  fprintf(stderr, "%s-%s():%d (%d)", __FILE__, __FUNCTION__, __LINE__, errno); \
  perror(message); \
}
#else
#define dperror(message) {}
#endif

#define BUFF_SIZE 4096

void printArgs(int argc, char **argv)
{
    int i;
    printf("%d - ", argc);
    for(i=0; i<argc; i++)
        printf("\"%s\" ", argv[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    pid_t ppid = 0;     // parrent pid
    char *str = NULL;   // path to cmdline of parent
    int fd = 0;         // fd for str
    char buf[BUFF_SIZE]; // read buffer
    struct stat sb;

    //printArgs(argc, argv);
    ppid = getppid();
    asprintf(&str, "/proc/%d/cmdline", ppid);

    if((fd = open(str, O_RDONLY)) != -1) {
        if(!fstat(fd, &sb)) {
            read(fd, &buf, BUFF_SIZE);     
        } else {
            dperror("fstat()");
        }
        close(fd);
    } else {
        dperror("open()");
    }

    openlog("sh-wrapper", LOG_PID, LOG_USER);
    syslog(5, "ppid=%d, calling prozess=\"%s\"", ppid, buf);
    closelog();

    // only +1 for 2 more args, because we strip the first arg from argv
    unsigned arg_more = 1;
    unsigned arg_num = argc + arg_more;
    char *tmp[arg_num + 1];
    tmp[0] = "sh";
    tmp[1] = "-f";
    //tmp[2] = "-x";
    for(int i=1; i<argc; i++) {
        tmp[i + arg_more] = argv[i];
    }
    tmp[arg_num] = NULL;
    //printArgs(arg_num, tmp);

    execv("/usr/bin/zsh", tmp);
    dperror("execl()");
    return EXIT_FAILURE;
}
