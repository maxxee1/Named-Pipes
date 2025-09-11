// g++ -std=c++17 guardian.cpp -o guardian
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
using namespace std;

int main(){
    string base = "/tmp/sochat";
    string uplink = base + "/guardian.in";
    mkdir(base.c_str(), 0777);
    mkfifo(uplink.c_str(), 0666);


    signal(SIGPIPE, SIG_IGN);

    
    int keep = open(uplink.c_str(), O_WRONLY | O_NONBLOCK);

    ifstream in(uplink);                            
    unordered_map<int,int> outfd;             
    unordered_map<int,string> names;                

    cerr << "[guardian] escuchando en " << uplink << "\n";

    string line;
    while (getline(in, line)) {
        // REGISTER <pid> <name...>
        if (line.rfind("REGISTER ", 0) == 0) {
            string junk, nm; long long pid;
            stringstream ss(line); ss >> junk >> pid; getline(ss, nm);
            if (!nm.empty() && nm[0]==' ') nm.erase(0,1);
            names[(int)pid] = nm.empty()? "anon" : nm;

            // canal de bajada para ese cliente: guardian -> cliente
            string down = base + "/guardian_to_" + to_string(pid);
            mkfifo(down.c_str(), 0666);                         // idempotente
            int fd = open(down.c_str(), O_WRONLY | O_NONBLOCK); // requiere que el cliente ya lo haya abierto en lectura
            if (fd >= 0) {
                outfd[(int)pid] = fd;
                string hi = "[central] hola " + names[(int)pid] + "\n";
                write(fd, hi.c_str(), hi.size());
            } else {
                perror("[guardian] open downlink");
            }
            continue;
        }

        // MSG <pid> <texto...>   (tu cliente ya formatea "[name@pid]: ...")
        if (line.rfind("MSG ", 0) == 0) {
            string junk, text; long long pid;
            stringstream ss(line); ss >> junk >> pid; getline(ss, text);
            if (!text.empty() && text[0]==' ') text.erase(0,1);
            if (text.empty()) continue;
            if (text.back() != '\n') text.push_back('\n');

            // broadcast a todos
            for (auto it = outfd.begin(); it != outfd.end(); ){
                if (write(it->second, text.c_str(), text.size()) < 0) {
                    close(it->second);
                    it = outfd.erase(it);  // cliente se fue
                } else {
                    ++it;
                }
            }
            continue;
        }

        // ignoramos cualquier otra cosa (p.ej., "reportar ...")
        // cerr << "[guardian] raw: " << line << "\n";
    }

    for (auto &kv : outfd) close(kv.second);
    if (keep >= 0) close(keep);
    return 0;
}
