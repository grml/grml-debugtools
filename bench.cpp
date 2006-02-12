/*
 * (c) Michael Gebetsroither <michael.geb@gmx.at>
 * gpl version 2 or any later version
 *
 */

#include <gebi/stopwatch.hpp>
using namespace gebi;

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>


static char* to_exec_;


void execChild()
{
    if(execlp(to_exec_, to_exec_, (char*)0) < 0)
        perror("execlp");
    exit(0);
}


int main(int argc, char *argv[])
{
    int status;
    pid_t child_pid;
    struct rusage ru;

    to_exec_ = NULL;
    if(argc == 2) {
        to_exec_ = argv[1];
    } else {
        fprintf(stderr, "Error: please give me the executable to benchmark\n");
        exit(1);
    }

    Stopwatch<> sw;
    child_pid = fork();
    if(0 == child_pid)
        execChild();
    sw.stop();
    if(wait4(child_pid, &status, 0, &ru) != child_pid)
        perror("wait4");
    printf("usr: %lds %ldns\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec);
    printf("sys: %lds %ldns\n", ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
    printf("ticks: %lld\n", sw.getTime());
    printf("pagereclaims:\t%ld\n", ru.ru_minflt);
    printf("pagefaults:\t%ld\n", ru.ru_majflt);
    return 0;
}
