// g++ -std=c++17 guardian.cpp -o guardian
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
using namespace std;

int main(){
    string base = "/tmp/sochat";
    string uplink = base + "/guardian.in";          // clientes -> guardian
    mkdir(base.c_str(), 0777);
    mkfifo(uplink.c_str(), 0666);

    signal(SIGPIPE, SIG_IGN);                       // no morir si un cliente se fue
    int keep = open(uplink.c_str(), O_WRONLY | O_NONBLOCK); // evita EOF sin escritores

    ifstream in(uplink);                            // bloquea hasta 1er escritor
    unordered_map<int,int> outfd;                  // pid -> fd(/guardian_to_<pid>)
    unordered_map<int,string> names;               // pid -> nombre

    cerr << "[guardian] escuchando en " << uplink << "\n";
    string line;
    while (getline(in, line)) {
        if (line.rfind("REGISTER ", 0) == 0) {
            // REGISTER <pid> <name...>
            string junk, nm; long long pid;
            stringstream ss(line); ss >> junk >> pid; getline(ss, nm);
            if (!nm.empty() && nm[0]==' ') nm.erase(0,1);
            names[(int)pid] = nm.empty()? "anon" : nm;

            string down = base + "/guardian_to_" + to_string(pid);   // guardian -> cliente
            mkfifo(down.c_str(), 0666);
            int fd = open(down.c_str(), O_WRONLY | O_NONBLOCK);
            if (fd >= 0) {
                outfd[(int)pid] = fd;
                string hi = "[central] hola " + names[(int)pid] + "\n";
                write(fd, hi.c_str(), hi.size());
            }
            continue;
        }

        if (line.rfind("MSG ", 0) == 0) {
            // MSG <pid> <texto...>  (texto ya viene en formato "[name@pid]: ...")
            string junk, text; long long pid;
            stringstream ss(line); ss >> junk >> pid; getline(ss, text);
            if (!text.empty() && text[0]==' ') text.erase(0,1);
            if (text.empty()) continue;
            if (text.back() != '\n') text.push_back('\n');

            // 1) "Muro": imprime en consola del central
            cerr << "[muro] " << text;  // muestra exactamente lo que llegó

            // 2) ACK inmediato al emisor (solo a ese pid)
            auto itSender = outfd.find((int)pid);
            if (itSender != outfd.end()) {
                string ack = "[central] recibido\n";
                write(itSender->second, ack.c_str(), ack.size());
            }

            // 3) Broadcast del mensaje a todos los demás (opcional: excluir emisor)
            for (auto it = outfd.begin(); it != outfd.end(); ) {
                if (it->first == (int)pid) { ++it; continue; } // no eco al emisor
                if (write(it->second, text.c_str(), text.size()) < 0) {
                    close(it->second); it = outfd.erase(it);   // cliente se fue
                } else ++it;
            }
            continue;
        }
        // otras líneas se ignoran (p.ej., "reportar ...")
    }

    for (auto &kv : outfd) close(kv.second);
    if (keep >= 0) close(keep);
    return 0;
}
