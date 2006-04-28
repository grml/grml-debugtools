#include "process.h"

#include <boost/lexical_cast.hpp>

#include <glib.h>


using std::string;


#define FREE(a) \
    do { \
        free(a); \
        a = NULL; \
    } while(false) \


namespace gebi
{

void process::init_priv()
{
    nop_ = "<none>";
    exe_ = nop_;
    name_ = nop_;
    args_.clear();
    args_length_ = 0;
    cpuid_ = 0;
    pid_ = 0;
}

void process::clear_priv()
{
    unsigned tmppid = pid_;
    init_priv();
    pid_ = tmppid;
}

void process::searchExe_priv()
{
    char *str = NULL;
    GError *err = NULL;
    string tmp;

    // path to process binary
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

void process::searchCmdline_priv()
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
    args_length_ = len - name_.size()+1 - args_.size();
}


void process::print() const
{
    fprintf(stderr, "%d ", pid_);
    fprintf(stderr, "%s ", exe_.c_str());
    fprintf(stderr, "%s\n", name_.c_str());
}

void process::searchInfos()
{
    clear_priv();
    searchExe_priv();
    searchCmdline_priv();
}

const string process::getArgs() const
{
    if(args_.empty())
        return nop_;

    string tmp;
    tmp.reserve(args_length_ + args_.size() * 3);
    for(unsigned i=0; i < args_.size(); i++) {
        //printf("size=%d\n", args_[i].size());
        tmp.append("\"");
        tmp.append(args_[i]);
        tmp.append("\"");
        //tmp.append(boost::lexical_cast<string>(args_[i].size()));
        if(i+1 < args_.size())
            tmp.append(" ");
    }
    /*
     fprintf(stderr, "getArgs(): args.size=%d, args_length_=%d, string.reserve=%d, \
          string.size=%d, string.capacity=%d\n", args_.size(), args_length_,
          args_length_ + args_.size() * 3, tmp.size(), tmp.capacity());
     */
    return tmp;
}

} // namespace 'gebi'

// vim: foldmethod=marker
