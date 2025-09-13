#include <bits/stdc++.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
using namespace std;


static void borrarEspacioInicial(string &s) {
    if(!s.empty() && s[0]==' ') {
        s.erase(0,1);
    }
}


static int abrirFifoEscritura(const string &path, int tries=30, int sleep_ms=50){
    for(int i=0;i<tries;i++) {
        int fd = open(path.c_str(), O_WRONLY | O_NONBLOCK);
        
        if(fd >= 0) {
            return fd;
        }

        if(errno==ENXIO || errno==ENOENT || errno==EAGAIN) {
            fd = open(path.c_str(), O_RDWR | O_NONBLOCK); // “sujeta” la FIFO
            if(fd >= 0) return fd;
            usleep(sleep_ms*1000);
            continue;
        }
        break; // otro error
    }
    return -1;
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    const string base      = "/tmp/sochat";
    const string pipeSubida    = base + "/guardian.in";         // clientes -> servidor
    const string toGuard   = base + "/srv_to_guard.fifo";       // servidor -> guardián (REPORT)
    const string fromGuard = base + "/guard_to_srv.fifo";       // guardián -> servidor (KILL)

    mkdir(base.c_str(), 0777);
    mkfifo(pipeSubida.c_str(),    0666);
    mkfifo(toGuard.c_str(),       0666);
    mkfifo(fromGuard.c_str(),     0666);

    signal(SIGPIPE, SIG_IGN);

    // Mantener escritor “dummy” del uplink para no recibir EOF cuando no hay clientes
    int keep = open(pipeSubida.c_str(), O_WRONLY | O_NONBLOCK);

    // Abrimos lectura NO bloqueante de ambos pipes
    int pipeEntrada = open(pipeSubida.c_str(), O_RDONLY | O_NONBLOCK);

    if(pipeEntrada < 0) {
        perror("[server] open uplink R");
        return 1;
    }

    int fd_from_guard = open(fromGuard.c_str(), O_RDONLY | O_NONBLOCK);

    if(fd_from_guard < 0) {
        perror("[server] open fromGuard R");
        return 1;
    }

    unordered_map<int,int> outfd;        // pid -> fd(/guardian_to_<pid>)
    unordered_map<int,string> names;     // pid -> nombre
    unordered_map<int,int> family_of;    // pid -> family_id
    unordered_map<int, unordered_set<int>> members; // family_id -> {pids}

    ofstream logf((base + "/chat.log").c_str(), ios::app);
    cerr << "[server] escuchando uplink: " << pipeSubida << "\n";

    string upBuf, guardBuf; upBuf.reserve(1<<14); guardBuf.reserve(1<<14);
    char tmp[2048];

    auto broadcast_except = [&](int exceptPid, const string &text) {
        for(auto it=outfd.begin(); it!=outfd.end(); ) {
            if(it->first == exceptPid) {
                ++it;
                continue;
            }

            if(write(it->second, text.c_str(), text.size()) < 0) {
                close(it->second); it = outfd.erase(it);
            }
            else
                ++it;
        }
    };

    auto remove_client = [&](int pid) {
        auto it = outfd.find(pid);

        if(it != outfd.end()) {
            close(it->second);
            outfd.erase(it);
        }

        auto fit = family_of.find(pid);
        
        if(fit != family_of.end()) {
            int fid = fit->second;
            family_of.erase(fit);
            auto &setp = members[fid];
            setp.erase(pid);

            if(setp.empty()) {
                members.erase(fid);
            }
        }

        names.erase(pid);
        string bye = string("[central] pid ") + to_string(pid) + " expulsado/desconectado\n";
        broadcast_except(pid, bye);
    };

    auto kill_family = [&](int victim) {
        // mata a todos los miembros de la familia del victim (si está conectado)
        auto fit = family_of.find(victim);

        if(fit == family_of.end()) return;  // si no está conectado, no hacemos nada
        int fid = fit->second;

        vector<int> group; group.reserve( (members.count(fid)? members[fid].size():0) );
        
        if (members.count(fid) > 0) {

            for (int pidMiembro : members[fid]) {
            group.push_back(pidMiembro);
            }
        }

        for(int p : group){
            // intenta terminarlo si sigue vivo
            kill(p, SIGTERM);
        }
        usleep(100000); // 100 ms
        for(int p : group){
            if(kill(p, 0) == 0) kill(p, SIGKILL);
            remove_client(p);
        }
    };

    auto handle_guard_line = [&](const string &gl) {
        if(gl.rfind("KILL|guardian|",0)==0) {
            vector<string> p; string t; stringstream ss(gl);

            while(getline(ss,t,'|')) p.push_back(t);

            if(p.size()==3) {
                int victim = atoi(p[2].c_str());
                kill_family(victim); // <-- KILL en cascada
            }
        }
    };

    auto handle_uplink_line = [&](const string &line) {
        // --- REGISTER ---
        if(line.rfind("REGISTER ",0)==0 || line.rfind("REGISTER|",0)==0) {
            int pid=0; string name; int fid=0;

            if(line.find('|')!=string::npos) {
                vector<string> p; string t; stringstream ss(line);

                while(getline(ss,t,'|')) p.push_back(t);

                // Format: REGISTER|pid|name  o  REGISTER|pid|name|family
                if(p.size()>=3) {
                    pid = atoi(p[1].c_str());
                    name = p[2];
                    if(p.size()>=4) fid = atoi(p[3].c_str());
                }
            }
            else
            {
                string junk, nm; long long ppid;
                stringstream ss(line); ss >> junk >> ppid; getline(ss, nm);
                borrarEspacioInicial(nm); pid=(int)ppid; name=nm;
            }

            if(fid == 0) fid = pid; // si no vino family, usa su propio pid

            names[pid] = name.empty()? "anon" : name;
            family_of[pid] = fid;
            members[fid].insert(pid);

            string pipeBajada = base + "/guardian_to_" + to_string(pid);
            mkfifo(pipeBajada.c_str(), 0666);
            int fd = abrirFifoEscritura(pipeBajada);

            if(fd >= 0) {
                outfd[pid] = fd;
                string hi = "[central] hola " + names[pid] + "\n";
                (void)write(fd, hi.c_str(), hi.size());
            }
            else
            {
                perror("[server] open downlink");
            }

            return;
        }

        // --- LEAVE ---
        if(line.rfind("LEAVE|",0)==0 || line.rfind("LEAVE ",0)==0) {
            int pid=0;

            if(line.find('|')!=string::npos) {
                vector<string> p; string t; stringstream ss(line);

                while(getline(ss,t,'|')) p.push_back(t);

                if(p.size()>=2) pid = atoi(p[1].c_str());
            }
            else
            {
                string junk; long long ppid; stringstream ss(line); ss>>junk>>ppid; pid=(int)ppid;
            }

            remove_client(pid);
            return;
        }

        // --- MSG ---
        if(line.rfind("MSG ",0)==0 || line.rfind("MSG|",0)==0) {
            int pid=0; string text;

            if(line.find('|')!=string::npos) {
                vector<string> p; string t; stringstream ss(line);

                while(getline(ss,t,'|')) p.push_back(t);

                if(p.size()>=3) {
                    pid=atoi(p[1].c_str());
                    text=p[2];
                }
            }
            else
            {
                string junk; long long ppid; stringstream ss(line); ss>>junk>>ppid; getline(ss,text);
                pid=(int)ppid; borrarEspacioInicial(text);
            }

            if(text.empty()) return;
            if(text.back()!='\n') text.push_back('\n');

            // muro + log
            cerr << "[muro] " << text;
            if(logf) {
                time_t now=time(nullptr);
                logf<<now<<" "<<text;
                logf.flush();
            }

            // ACK al emisor
            auto itSender = outfd.find(pid);
            if(itSender!=outfd.end()) {
                string ack = "[central] recibido\n";
                (void)write(itSender->second, ack.c_str(), ack.size());
            }

            // Broadcast a todos menos el emisor
            for(auto it=outfd.begin(); it!=outfd.end(); ) {

                if(it->first == pid) {
                    ++it;
                    continue;
                }

                if(write(it->second, text.c_str(), text.size()) < 0) {
                    close(it->second); it = outfd.erase(it);
                }
                else
                ++it;
            }

            return;
        }

        // --- REPORT ---
        if(line.rfind("REPORT ",0)==0 || line.rfind("REPORT|",0)==0 ||
           line.rfind("reportar ",0)==0) {
            int rep=0, tar=0;

            if(line.find('|')!=string::npos) {
                vector<string> p; string t; stringstream ss(line);

                while(getline(ss,t,'|')) p.push_back(t);

                if(p.size()>=3) {
                    rep=atoi(p[1].c_str());
                    tar=atoi(p[2].c_str());
                }

            }
            else
            {
                string mensaje; long long a=0,b=0; stringstream ss(line); ss>>mensaje>>a>>b;
                if(mensaje=="REPORT" && b) {
                    rep=(int)a;
                    tar=(int)b;
                }
            }

            int fd_to_guard = open(toGuard.c_str(), O_WRONLY | O_NONBLOCK);

            if(fd_to_guard >= 0) {
                string msg = "REPORT|" + to_string(rep) + "|" + to_string(tar) + "\n";
                (void)write(fd_to_guard, msg.c_str(), msg.size());
                close(fd_to_guard);
            }

            return;
        }
        // Otros comandos se ignoran
    };

    auto reopen_fifo_rd_nonblock = [&](int &fd, const string &path) {
        close(fd);
        fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);

        if(fd < 0) perror("[server] reopen FIFO");
    };

    auto pump_fd_lines = [&](int &fd, const string &path, string &acc, auto &&handler) {

        while(true) {
            ssize_t n = read(fd, tmp, sizeof(tmp));

            if(n > 0) {
                acc.append(tmp, tmp+n);
            }
            else if(n == 0) {
                reopen_fifo_rd_nonblock(fd, path);
                break;
            }
            else
            {
                if(errno==EAGAIN || errno==EWOULDBLOCK) break;
                if(errno==EINTR) continue;
                perror("[server] read FIFO"); break;
            }

        }

        size_t pos;

        while((pos = acc.find('\n')) != string::npos) {
            string L = acc.substr(0,pos);
            acc.erase(0,pos+1);
            handler(L);
        }
    };

    while(true) {

        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(pipeEntrada, &rfds);
        FD_SET(fd_from_guard, &rfds);
        int maxfd = max(pipeEntrada, fd_from_guard);
        timeval tv{0, 200000}; // 200 ms
        int r = select(maxfd+1, &rfds, nullptr, nullptr, &tv);
    
        if(r < 0) {
            if(errno==EINTR) continue;
            perror("[server] select"); break;
        }
        if(FD_ISSET(fd_from_guard, &rfds))
            pump_fd_lines(fd_from_guard, fromGuard, guardBuf, handle_guard_line);
        if(FD_ISSET(pipeEntrada, &rfds))
            pump_fd_lines(pipeEntrada, pipeSubida,    upBuf,    handle_uplink_line);
    }

    for(auto &kv: outfd) close(kv.second);
    if(fd_from_guard >= 0) close(fd_from_guard);
    if(pipeEntrada   >= 0) close(pipeEntrada);
    if(keep          >= 0) close(keep);
    return 0;
}
