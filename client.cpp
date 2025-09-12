// client.cpp — Proceso participante (dup, reportar, salir). Hijo muere con el padre.
// Compila: g++ -std=c++17 -Wall -Wextra -O2 client.cpp -o client
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
using namespace std;

static string base   = "/tmp/sochat";
static string uplink = base + "/guardian.in";  // cliente -> servidor (uplink común)

static void sendLine(int fd_up, const string &s){
    string line = s; if(line.empty() || line.back()!='\n') line.push_back('\n');
    (void)write(fd_up, line.c_str(), line.size());
}

int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.setf(std::ios::unitbuf); // flush automático

    signal(SIGPIPE, SIG_IGN);
    mkdir(base.c_str(), 0777);

    // args: ./client <name> [--family <id>]
    string name = (argc>=2 ? argv[1] : string("user_")+to_string(getpid()));
    int family_id = 0;
    for(int i=2;i<argc;i++){
        if(string(argv[i])=="--family" && i+1<argc){
            family_id = atoi(argv[i+1]); i++;
        }
    }

    int mypid = (int)getpid();
    if(family_id == 0) family_id = mypid; // si no vino family, usa su propio pid

    // IMPORTANTE: si este proceso es hijo (lanzado por dup), queremos que muera con el padre
    // (solo funciona en Linux): al morir el padre, el kernel nos envía SIGTERM.
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    // FIFO de bajada (servidor -> cliente) ANTES de registrar
    string down = base + "/guardian_to_" + to_string(mypid);
    mkfifo(down.c_str(), 0666);
    int fd_down = open(down.c_str(), O_RDONLY | O_NONBLOCK);
    if(fd_down < 0){ perror("[client] open down"); return 1; }

    // Uplink (bloquea hasta que el server esté leyendo)
    int fd_up = open(uplink.c_str(), O_WRONLY);
    if(fd_up < 0){ perror("[client] open uplink"); return 1; }

    // REGISTER: enviamos también el family_id (4to campo)
    sendLine(fd_up, "REGISTER|"+to_string(mypid)+"|"+name+"|"+to_string(family_id));

    cout << "[client] PID=" << mypid << " name=" << name
         << " comandos: (texto normal) | reportar <pid> | dup | salir" << endl;

    string inbuf; inbuf.reserve(4096);
    char tmp[1024];

    while(true){
        // 1) Leer mensajes del servidor (no bloquea)
        ssize_t n = read(fd_down, tmp, sizeof(tmp));
        if(n > 0){
            inbuf.append(tmp, tmp+n);
            size_t pos;
            while((pos = inbuf.find('\n')) != string::npos){
                string L = inbuf.substr(0,pos);
                inbuf.erase(0,pos+1);
                cout << L << endl;
            }
        }

        // 2) Esperar entrada por teclado con select (200ms)
        fd_set rfds; FD_ZERO(&rfds); FD_SET(STDIN_FILENO, &rfds);
        timeval tv{0, 200000};
        int r = select(STDIN_FILENO+1, &rfds, nullptr, nullptr, &tv);
        if(r > 0 && FD_ISSET(STDIN_FILENO, &rfds)){
            string cmd;
            if(!getline(cin, cmd)){ sendLine(fd_up, "LEAVE|"+to_string(mypid)+"|"); break; }

            if(cmd == "salir"){
                sendLine(fd_up, "LEAVE|"+to_string(mypid)+"|");
                break;

            }else if(cmd.rfind("reportar ",0)==0){
                string w; int target=0; stringstream ss(cmd); ss>>w>>target;
                if(target>0) sendLine(fd_up, "REPORT|"+to_string(mypid)+"|"+to_string(target));

            }else if(cmd == "dup"){
                // Lanza un proceso hijo que herede el mismo family_id
                pid_t child = fork();
                if(child == 0){
                    // (no setsid) — queremos que el hijo pueda morir con el padre si este muere
                    string cname = name + "_dup";
                    execlp("./client", "./client", cname.c_str(), "--family", to_string(family_id).c_str(), (char*)nullptr);
                    perror("[client-child] execlp"); _exit(1);
                }else if(child > 0){
                    cout << "[client] duplicado lanzado con PID=" << child << endl;
                }else{
                    perror("[client] fork");
                }

            }else{
                string payload = "[" + name + "@" + to_string(mypid) + "]: " + cmd;
                sendLine(fd_up, "MSG|"+to_string(mypid)+"|"+payload);
            }
        }
    }

    close(fd_down);
    close(fd_up);
    return 0;
}
