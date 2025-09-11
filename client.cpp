// g++ -std=c++17 cliente.cpp -o cliente
#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

static inline void print_menu(pid_t pid, const string& name){
    fprintf(stderr,
        "=== Chat Cliente (pid=%d, name=%s) ===\n"
        "bienvenido\n"
        "para volver a desplegar este menu ingresa /menu\n"
        "dejar el chat            /leave\n"
        "reportar este proceso    /report\n"
        "reportar otro proceso    /imma_snitching_big_rat <pid>\n"
        "-----------------------------\n", pid, name.c_str());
}


int main(int argc, char** argv)
{
    string base = "/tmp/sochat/";
    string uplink = base + "/guardian.in";
    pid_t pid = getpid();

    int fd_out = open(uplink.c_str(), O_WRONLY);
    if (fd_out < 0) { perror("open guardian.in"); return 1; }


    string downlink = base + "/guardian_to_" + to_string(pid);
    mkfifo(downlink.c_str(), 0666);                       
    int fd_in = open(downlink.c_str(), O_RDONLY | O_NONBLOCK); 
    if (fd_in < 0) { perror("open downlink"); }


    string name = (argc >= 2) ? argv[1] : "example";

    {
        string reg = "REGISTER " + to_string(pid) + " " + name + "\n";
        write(fd_out, reg.c_str(), reg.size());
    }


    print_menu(pid, name);  

    string line;
    while (getline(cin, line)) {
        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        if (fd_in >= 0) FD_SET(fd_in, &rfds);
        int maxfd = (fd_in >= 0) ? max(STDIN_FILENO, fd_in) : STDIN_FILENO;

        if (select(maxfd + 1, &rfds, nullptr, nullptr, nullptr) < 0) break;

        // 1) Mensajes desde el central (broadcast/eco)
        if (fd_in >= 0 && FD_ISSET(fd_in, &rfds)) {
            char buf[1024];
            ssize_t n = read(fd_in, buf, sizeof(buf) - 1);
            if (n > 0) { buf[n] = '\0'; cout << buf << flush; }
            // si n==0 o error, lo ignoramos en esta versión simple
        }

        // 2) Entrada del usuario
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            string line;
            if (!getline(cin, line)) break;      // EOF (Ctrl+D)

           if (line == "/menu") {               // solo reimprime menú
                print_menu(pid, name);
                continue;
            }
            if (line == "/leave") {
                string xao = "MSG " + to_string(pid) + " [leave]\n";  // <- variable xao
                (void)write(fd_out, xao.c_str(), xao.size());
                break;
            }

            // Mensaje normal (sin reportes)
            string msg = "MSG " + to_string(pid) + " [" + name + "@" + to_string(pid) + "]: " + line + "\n";
            (void)write(fd_out, msg.c_str(), msg.size());
        }
    }

}
