/*
 * (c) Michael Gebetsroither <michael.geb@gmx.at>
 * gpl version 2 or any later version
 *
 */

#include "process.h"

#include <map>
#include <string>
#include <algorithm>
#include <iostream>

#include <glib.h>
#include <glib/gstdio.h>

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <stdio.h>
#include <stdlib.h>     /* exit() */
#include <signal.h>
#include <syslog.h>     /* syslog() */
#include <unistd.h>     /* sysconf(), setsid(), fork(), dup(), close() */
#include <string.h>     /* strerror() */
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/types.h>      /* fork(), open(), waitpid(), IPC */

#include <linux/netlink.h>
#include <linux/cn_proc.h>
#include <linux/connector.h>

using gebi::process;

using std::map;
using std::string;
using std::ifstream;
using std::cin;
using std::cout;

using boost::shared_ptr;

#define MESSAGE_SIZE    (sizeof(struct nlmsghdr) + \
                         sizeof(struct cn_msg)   + \
                         sizeof(int))
#define FREE(a) \
    do { \
        free(a); \
        a = NULL; \
    } while(false) \


const unsigned PROC_EVENT_NONE = 0x00000000;
const unsigned PROC_EVENT_FORK = 0x00000001;
const unsigned PROC_EVENT_EXEC = 0x00000002;
const unsigned PROC_EVENT_UID  = 0x00000004;
const unsigned PROC_EVENT_GID  = 0x00000040;
const unsigned PROC_EVENT_EXIT = 0x80000000;


class process;
typedef shared_ptr<process> process_ptr;
typedef map<unsigned, process_ptr> pmap;

int sk_nl;   /* socket used by netlink */
bool exit_now;  /* used by signal handler */
pmap data_;


process_ptr get_add_process(unsigned pid, bool update)
{
    if(data_.find(pid) == data_.end()) {
        process_ptr tmp(new process(pid));
        if(update)
            tmp->searchInfos();
        data_[pid] = tmp;
        return tmp;
    }
    return data_[pid];
}

 /* cn_fork_listen - {{{
 */
void cn_fork_listen(int sock)
{
    char buff[128];     /* must be > MESSAGE_SIZE */
    struct nlmsghdr *hdr;
    struct cn_msg *msg;     
    int err;

    /* Clear the buffer */
    memset(buff, '\0', sizeof(buff));

    /* fill the message header */
    hdr = (struct nlmsghdr *) buff;

    hdr->nlmsg_len = MESSAGE_SIZE;
    hdr->nlmsg_type = NLMSG_DONE;
    hdr->nlmsg_flags = 0;
    hdr->nlmsg_seq = 0;
    hdr->nlmsg_pid = getpid();

    /* the message */
    msg = (struct cn_msg *) NLMSG_DATA(hdr);
    msg->id.idx = CN_IDX_PROC;
    msg->id.val = CN_VAL_PROC;
    msg->seq = 0;
    msg->ack = 0;
    msg->len = sizeof(int);
    msg->data[0] = PROC_CN_MCAST_LISTEN;

    err = send(sock, hdr, hdr->nlmsg_len, 0);
    if(-1 == err) {
        perror("send()");
    }
} // }}}

/* cn_fork_ignore - {{{
 */
static inline void cn_fork_ignore(int sock)
{
    char buff[128];     /* must be > MESSAGE_SIZE */
    struct nlmsghdr *hdr;
    struct cn_msg *msg;

    /* Clear the buffer */
    memset(buff, '\0', sizeof(buff));

    /* fill the message header */
    hdr = (struct nlmsghdr *) buff;

    hdr->nlmsg_len = MESSAGE_SIZE;
    hdr->nlmsg_type = NLMSG_DONE;
    hdr->nlmsg_flags = 0;
    hdr->nlmsg_seq = 0;
    hdr->nlmsg_pid = getpid();

    /* the message */
    msg = (struct cn_msg *) NLMSG_DATA(hdr);
    msg->id.idx = CN_IDX_PROC;
    msg->id.val = CN_VAL_PROC;
    msg->seq = 0;
    msg->ack = 0;
    msg->len = sizeof(int);
    msg->data[0] = PROC_CN_MCAST_IGNORE;

    send(sock, hdr, hdr->nlmsg_len, 0);
} // }}}

void eventFork(struct nlmsghdr *hdr)
{
    struct cn_msg *msg = (struct cn_msg *)NLMSG_DATA(hdr);
    struct proc_event *ev = (struct proc_event *)msg->data;
    unsigned ppid, ptgid, cpid, ctgid;
    process_ptr tp;

    ppid  = ev->event_data.fork.parent_pid;
    ptgid = ev->event_data.fork.parent_tgid;
    cpid  = ev->event_data.fork.child_pid;
    ctgid = ev->event_data.fork.child_tgid;
    tp = get_add_process(ppid, true);

    if(tp->getExe() == tp->getName())
        printf("fork: %d exe=%s ppid=%d ptgid=%d - cpid=%d ctgid=%d\n",
            msg->seq, tp->getExe().c_str(),
            ppid, ptgid, cpid, ctgid);
    else
        printf("fork: %d exe=%s name=%s ppid=%d ptgid=%d - cpid=%d ctgid=%d\n",
            msg->seq, tp->getExe().c_str(), tp->getName().c_str(),
            ppid, ptgid, cpid, ctgid);
    process_ptr tmp(new process(cpid));
    tmp->searchInfos();
    data_[cpid] = tmp; 
}

void eventExec(struct nlmsghdr *hdr)
{
    struct cn_msg *msg = (struct cn_msg *)NLMSG_DATA(hdr);
    struct proc_event *ev = (struct proc_event *)msg->data;
    unsigned ppid, ptgid;
    process_ptr tp;

    ppid = ev->event_data.exec.process_pid;
    ptgid = ev->event_data.exec.process_tgid;
    tp = get_add_process(ppid, false);
    tp->searchInfos();

    if(tp->getExe() == tp->getName())
        printf("exec: %d exe=%s args=\"%s\" pid=%d tgid=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getArgs().c_str(),
                ppid, ptgid);
    else
        printf("exec: %d exe=%s name=%s args=\"%s\" pid=%d tgid=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getName().c_str(), tp->getArgs().c_str(),
                ppid, ptgid);
}

void eventUid(struct nlmsghdr *hdr)
{
    struct cn_msg *msg = (struct cn_msg *)NLMSG_DATA(hdr);
    struct proc_event *ev = (struct proc_event *)msg->data;
    unsigned ppid, ptgid, ruid, euid;
    process_ptr tp;

    ppid = ev->event_data.id.process_pid;
    ptgid = ev->event_data.id.process_tgid;
    ruid = ev->event_data.id.r.ruid;
    euid = ev->event_data.id.e.euid;
    tp = get_add_process(ppid, true);

    if(tp->getExe() == tp->getName())
        printf("uid: %d exe=%s pid=%d tgid=%d - ruid=%d euid=%d\n",
                msg->seq, tp->getExe().c_str(), ppid, ptgid, ruid, euid);
    else
        printf("uid: %d exe=%s name=%s pid=%d tgid=%d - ruid=%d euid=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getName().c_str(),
                ppid, ptgid, ruid, euid);
}

void eventGid(struct nlmsghdr *hdr)
{
    struct cn_msg *msg = (struct cn_msg *)NLMSG_DATA(hdr);
    struct proc_event *ev = (struct proc_event *)msg->data;
    unsigned ppid, ptgid, rgid, egid;
    process_ptr tp;

    ppid = ev->event_data.id.process_pid;
    ptgid = ev->event_data.id.process_tgid;
    rgid = ev->event_data.id.r.rgid;
    egid = ev->event_data.id.e.egid;
    tp = get_add_process(ppid, true);

    if(tp->getExe() == tp->getName())
        printf("gid: %d exe=%s pid=%d tgid=%d - rgid=%d egid=%d\n",
                msg->seq, tp->getExe().c_str(), ppid, ptgid, rgid, egid);
    else
        printf("gid: %d exe=%s name=%s pid=%d tgid=%d - rgid=%d egid=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getName().c_str(),
                ppid, ptgid, rgid, egid);
}

void eventExit(struct nlmsghdr *hdr)
{
    struct cn_msg *msg = (struct cn_msg *)NLMSG_DATA(hdr);
    struct proc_event *ev = (struct proc_event *)msg->data;
    unsigned ppid, ptgid, ec, es;
    process_ptr tp;

    ppid = ev->event_data.exit.process_pid;
    ptgid = ev->event_data.exit.process_tgid;
    ec = ev->event_data.exit.exit_code;
    es = ev->event_data.exit.exit_signal;
    tp = get_add_process(ppid, true);

    if(tp->getExe() == tp->getName())
        printf("exit: %d exe=%s pid=%d tgid=%d - exitcode=%d exitsignal=%d\n",
                msg->seq, tp->getExe().c_str(), ppid, ptgid, ec, es);
    else
        printf("exit: %d exe=%s name=%s pid=%d tgid=%d - exitcode=%d exitsignal=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getName().c_str(),
                ppid, ptgid, ec, es);
    data_.erase(ppid);
}

void recv_sk_nl(int sk)
{
    char buff[CONNECTOR_MAX_MSG_SIZE];    /* it's large enough */
    struct nlmsghdr *hdr;
    struct cn_msg *msg;
    struct proc_event *ev;
    int len;

    /* Clear the buffer */
    memset(buff, '\0', sizeof(buff));

    /* Listen */
    len = recv(sk, buff, sizeof(buff), 0);
    if (len == -1) {
        syslog(LOG_INFO, "netlink recv error");
        return;
    }

    /* point to the message header */
    hdr = (struct nlmsghdr *)buff;

    switch (hdr->nlmsg_type) {
        case NLMSG_DONE:
            msg = (struct cn_msg *)NLMSG_DATA(hdr);
            ev = (struct proc_event *)msg->data;
            switch(ev->what) {
                case PROC_EVENT_FORK:
                   eventFork(hdr);
                   break;
                case PROC_EVENT_EXEC:
                   eventExec(hdr);
                   break;
                case PROC_EVENT_UID:
                   eventUid(hdr);
                   break;
                case PROC_EVENT_GID:
                   eventGid(hdr);
                   break;
                case PROC_EVENT_EXIT:
                   eventExit(hdr);
                   break;
                case PROC_EVENT_NONE:
                   printf("none\n");
                   break;
                default:
                   printf("unknown event\n");
            }
            break;
        case NLMSG_ERROR:
            syslog(LOG_INFO, "recv_sk_nl error");
    }
}


void termSig(int x)
{
    exit_now = true;
}


int main()
{
try {
    int err;
    struct sockaddr_nl sa_nl;   /* info for the netlink interface */
    fd_set fds;     /* file descriptor set */
    int max_fds;
    exit_now = false;

    // install signal handler
    signal(SIGINT, termSig);
    signal(SIGTERM, termSig);

    sk_nl = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (sk_nl == -1) {
        perror("socked()");
        syslog(LOG_INFO, "socket sk_nl error");
        return -1;
    }
    sa_nl.nl_family = AF_NETLINK;
    sa_nl.nl_groups = CN_IDX_PROC;
    sa_nl.nl_pid    = getpid();

    err = bind(sk_nl, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
    if (err == -1) {
        perror("bind()");
        syslog(LOG_INFO, "binding sk_nl error");
        close(sk_nl);
        return -1;
    }

    /* We're ready to receive cn_fork netlink message */
    cn_fork_listen(sk_nl);

    max_fds = sk_nl + 1;

    for (;;) {
        /* waiting for sk_nl socket changes */
        FD_ZERO(&fds);
        FD_SET(sk_nl, &fds);

        if(exit_now)
            break;

        err = select(max_fds, &fds, NULL, NULL, NULL);

        if (err < 0) {
            syslog(LOG_INFO, " !!! select error");
            continue;
        }

        try
        {
            if (FD_ISSET(sk_nl, &fds))
                recv_sk_nl(sk_nl);
        }
        catch (std::exception &e)
        {
            fflush(stdout);
            fprintf(stderr, "Exception cought: %s\n", e.what());
        }
    }

    cn_fork_ignore(sk_nl);
    data_.clear();

} catch (std::exception &e) {
    fflush(stdout);
    fprintf(stderr, "Exception cought: %s\n", e.what());
}

    fflush(stdout);
    return EXIT_SUCCESS;
}

// vim: foldmethod=marker
