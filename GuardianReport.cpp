#include <bits/stdc++.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
using namespace std;

int main() {

    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    const string base = "/tmp/sochat";
    const string from_srv = base + "/srv_to_guard.fifo";   // servidor -> guardián
    const string to_srv   = base + "/guard_to_srv.fifo";   // guardián -> servidor
    mkdir(base.c_str(), 0777);
    mkfifo(from_srv.c_str(), 0666);
    mkfifo(to_srv.c_str(),   0666);

    int fd_in  = open(from_srv.c_str(), O_RDONLY);

    if(fd_in  < 0) {
        perror("[guardian] open from_srv");
        return 1;
    }

    int fd_out = open(to_srv.c_str(),   O_WRONLY);

    if(fd_out < 0) {
        perror("[guardian] open to_srv");
        return 1;
    }

    unordered_map<int,int> strikes;
    string buf; buf.reserve(4096);
    char tmp[1024];

    auto flush_lines = [&](string &acc) {
        size_t pos;

        while((pos = acc.find('\n')) != string::npos) {

            string line = acc.substr(0,pos);
            acc.erase(0,pos+1);

            // Esperamos: REPORT|rep|tar
            if(line.rfind("REPORT|",0)==0) {
                vector<string> parts; string t;
                stringstream ss(line);

                while(getline(ss,t,'|')) parts.push_back(t);

                if(parts.size()==3) {
                    int target = atoi(parts[2].c_str());

                    if(++strikes[target] > 9) {
                        string killMsg = "KILL|guardian|" + to_string(target) + "\n";
                        (void)write(fd_out, killMsg.c_str(), killMsg.size());
                        strikes.erase(target); // reset para ese pid
                    }
                }
            }
        }
    };

    while(true) {
        ssize_t n = read(fd_in, tmp, sizeof(tmp));

        if(n > 0) {
            buf.append(tmp, tmp+n);
            flush_lines(buf);
        }
        else if(n == 0) {
            close(fd_in);
            fd_in = open(from_srv.c_str(), O_RDONLY);

            if(fd_in < 0) {
                perror("[guardian] reopen from_srv");
                break;
            }
        }
        else
        {
            if(errno==EINTR) continue;
            perror("[guardian] read"); break;
        }
    }

    close(fd_in); close(fd_out);
    return 0;
}
