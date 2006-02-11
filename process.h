#ifndef __process_h__
#define __process_h__

#include <string>
#include <vector>

namespace gebi
{

class process
{
    private:
        unsigned cpuid_;
        unsigned pid_;
        std::string exe_;
        std::string name_;
        std::vector<std::string> args_;
        std::string nop_;

        void init_priv();
        void clear_priv();
        void searchExe_priv();
        void searchCmdline_priv();

    public:
        process() { init_priv(); }
        process(unsigned pid) { init_priv(); pid_ = pid; }
        ~process() {}

        void print();
        void searchInfos();
        const std::string &getExe() const { return exe_; }
        const std::string &getName() const { return name_; }
        const std::string getArgs() const;

        void setPid(unsigned pid) { pid_=pid; }
        unsigned getPid(void) const { return pid_; }
};

} // namespace 'gebi'

// vim: foldmethod=marker
#endif
