#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>

#include <glib.h>
#include <glib/gstdio.h>

#include <boost/lexical_cast.hpp>

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


using std::map;
using std::vector;
using std::string;
using std::ifstream;
using std::cin;
using std::cout;

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


int sk_nl;   /* socket used by netlink */
bool exit_now;  /* used by signal handler */
class prozess;
typedef map<unsigned, prozess*> pmap;
pmap data_;


class prozess
{
    private:
        unsigned cpuid_;
        unsigned pid_;
        string exe_;
        string name_;
        vector<string> args_;
        string nop_;

        void init_priv()
        {
            exe_ = nop_;
            name_ = nop_;
            args_.clear();
            cpuid_ = 0;
            pid_ = 0;
        }

        void clear_priv()
        {
            unsigned tmppid = pid_;
            init_priv();
            pid_ = tmppid;
        }

        void searchExe_priv()
        {
            char *str = NULL;
            GError *err = NULL;
            string tmp;

            // path to prozess binary
            tmp.append("/proc/");
            tmp.append(boost::lexical_cast<string>(pid_));
            tmp.append("/exe");
            str = g_file_read_link(tmp.c_str(), &err);
            if(!str) {
                //fprintf(stderr, "Error: %s (%d)\n", err->message, err->code);
                g_error_free(err);
                return;
            }
            exe_ = str;
            FREE(str);
        }

        void searchCmdline_priv()
        {
            char *str = NULL;
            char *tmpstr = NULL;
            GError *err = NULL;
            string tmp;
            unsigned len = 0;

            // get cmdline
            tmp = "/proc/";
            tmp.append(boost::lexical_cast<string>(pid_));
            tmp.append("/cmdline");
            if(!g_file_get_contents(tmp.c_str(), &str, &len, &err)) {
                //fprintf(stderr, "Error: %s (%d)\n", err->message, err->code);
                g_error_free(err);
                return;
            }

            tmpstr = str;
            //fprintf(stderr, "BEGIN\nstr=\"%s\" tmpstr=\"%s\"\n", str, tmpstr);
            unsigned i = 0;
            do {
                unsigned tmplen;
                args_.push_back(tmpstr);
                tmplen = strlen(tmpstr) + 1;
                i += tmplen;
                tmpstr += tmplen;
                //fprintf(stderr, "tmpstr=\"%s\" len=%d i=%d\n", tmpstr, tmplen, i);
            } while(i < len);
            name_ = args_.front();
            args_.erase(args_.begin());
            //fprintf(stderr, "len=%d strlen=%d cont=\"%s\"\nEND\n", len, strlen(str), str);
            tmpstr = NULL;
            FREE(str);
        }


    public:
        prozess() { nop_ = "<none>"; init_priv(); }
        prozess(unsigned pid) { nop_ = "<none>"; init_priv(); pid_ = pid; }
        ~prozess() {}

        void print()
        {
            fprintf(stderr, "%d ", pid_);
            fprintf(stderr, "%s ", exe_.c_str());
            fprintf(stderr, "%s\n", name_.c_str());
        }

        void searchInfos()
        {
            clear_priv();
            searchExe_priv();
            searchCmdline_priv();
        }

        const string &getExe() { return exe_; }
        const string &getName() { return name_; }
        const string getArgs()
        {
            if(args_.empty())
                return nop_;

            string tmp;
            for(unsigned i=0; i < args_.size(); i++) {
                //printf("size=%d\n", args_[i].size());
                tmp.append("\"");
                tmp.append(args_[i]);
                tmp.append("\"");
                //tmp.append(boost::lexical_cast<string>(args_[i].size()));
                if(i+1 < args_.size())
                    tmp.append(" ");
            }
            //return *new string(tmp);
            return string(tmp);
        }

        void setPid(unsigned pid) { pid_=pid; }
        unsigned getPid(void) { return pid_; }
};



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
    prozess *tp = NULL;

    ppid  = ev->event_data.fork.parent_pid;
    ptgid = ev->event_data.fork.parent_tgid;
    cpid  = ev->event_data.fork.child_pid;
    ctgid = ev->event_data.fork.child_tgid;
    if(data_.find(ppid) == data_.end()) {
        tp = new prozess(ppid);
        tp->searchInfos();
        data_[ppid] = tp;
        tp = NULL;
    }
    tp = data_[ppid];
    if(tp->getExe() == tp->getName())
        printf("fork: %d exe=%s ppid=%d ptgid=%d - cpid=%d ctgid=%d\n",
            msg->seq, tp->getExe().c_str(),
            ppid, ptgid, cpid, ctgid);
    else
        printf("fork: %d exe=%s name=%s ppid=%d ptgid=%d - cpid=%d ctgid=%d\n",
            msg->seq, tp->getExe().c_str(), tp->getName().c_str(),
            ppid, ptgid, cpid, ctgid);
    tp = new prozess(cpid);
    tp->searchInfos();
    data_[cpid] = tp; 
}

void eventExec(struct nlmsghdr *hdr)
{
    struct cn_msg *msg = (struct cn_msg *)NLMSG_DATA(hdr);
    struct proc_event *ev = (struct proc_event *)msg->data;
    unsigned ppid, ptgid;
    prozess *tp = NULL;

    ppid = ev->event_data.exec.process_pid;
    ptgid = ev->event_data.exec.process_tgid;
    if(data_.find(ppid) == data_.end()) {
        tp = new prozess(ppid);
    } else {
        tp = data_[ppid];
    }
    tp->searchInfos();
    if(tp->getExe() == tp->getName())
        printf("exec: %d exe=%s args=\"%s\" pid=%d tgid=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getArgs().c_str(),
                ppid, ptgid);
    else
        printf("exec: %d exe=%s name=%s args=\"%s\" pid=%d tgid=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getName().c_str(), tp->getArgs().c_str(),
                ppid, ptgid);
    data_[ppid] = tp;
}

void eventUid(struct nlmsghdr *hdr)
{
    struct cn_msg *msg = (struct cn_msg *)NLMSG_DATA(hdr);
    struct proc_event *ev = (struct proc_event *)msg->data;
    unsigned ppid, ptgid, ruid, euid;
    prozess *tp = NULL;

    ppid = ev->event_data.id.process_pid;
    ptgid = ev->event_data.id.process_tgid;
    ruid = ev->event_data.id.r.ruid;
    euid = ev->event_data.id.e.euid;
    if(data_.find(ppid) == data_.end()) {
        tp = new prozess(ppid);
        tp->searchInfos();
        data_[ppid] = tp;
    } else {
        tp = data_[ppid];
    }
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
    prozess *tp = NULL;

    ppid = ev->event_data.id.process_pid;
    ptgid = ev->event_data.id.process_tgid;
    rgid = ev->event_data.id.r.rgid;
    egid = ev->event_data.id.e.egid;
    if(data_.find(ppid) == data_.end()) {
        tp = new prozess(ppid);
        tp->searchInfos();
        data_[ppid] = tp;
    } else {
        tp = data_[ppid];
    }
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
    prozess *tp = NULL;

    ppid = ev->event_data.exit.process_pid;
    ptgid = ev->event_data.exit.process_tgid;
    ec = ev->event_data.exit.exit_code;
    es = ev->event_data.exit.exit_signal;
    if(data_.find(ppid) == data_.end()) {
        tp = new prozess(ppid);
        tp->searchInfos();
        data_[ppid] = tp;
    } else {
        tp = data_[ppid];
    }
    if(tp->getExe() == tp->getName())
        printf("exit: %d exe=%s pid=%d tgid=%d - exitcode=%d exitsignal=%d\n",
                msg->seq, tp->getExe().c_str(), ppid, ptgid, ec, es);
    else
        printf("exit: %d exe=%s name=%s pid=%d tgid=%d - exitcode=%d exitsignal=%d\n",
                msg->seq, tp->getExe().c_str(), tp->getName().c_str(),
                ppid, ptgid, ec, es);
    delete data_[ppid];
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
    fprintf(stderr, "\nexit signal!!\n");
    exit_now = true;
}


int main()
{
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

        err = select(max_fds, &fds, NULL, NULL, NULL);

        if(exit_now)
            break;

        if (err < 0) {
            syslog(LOG_INFO, " !!! select error");
            continue;
        }

        if (FD_ISSET(sk_nl, &fds))
            recv_sk_nl(sk_nl);
    }

    fprintf(stderr, "cleaning-up now: ");
    cn_fork_ignore(sk_nl);
    for(pmap::iterator i = data_.begin(); i != data_.end(); i++)
        delete((*i).second);
    data_.clear();
    fprintf(stderr, "done.\n");

    return EXIT_SUCCESS;
}

// vim: foldmethod=marker
