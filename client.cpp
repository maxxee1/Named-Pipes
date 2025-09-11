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
        "-----------------------------\n", pid, name.c_str());
}

int main(int argc, char** argv){
    string base   = "/tmp/sochat";
    string uplink = base + "/guardian.in";                     // yo -> guardian

    string name = (argc >= 2) ? argv[1] : "example";
    pid_t pid = getpid();

    // downlink personal (guardian -> yo): créalo y ábrelo ANTES de registrarte
    string downlink = base + "/guardian_to_" + to_string(pid);
    mkfifo(downlink.c_str(), 0666);
    int fd_in = open(downlink.c_str(), O_RDONLY | O_NONBLOCK);

    int fd_out = open(uplink.c_str(), O_WRONLY);               // requiere guardian leyendo
    if (fd_out < 0) { perror("open guardian.in"); return 1; }

    // REGISTRO
    {
        string reg = "REGISTER " + to_string(pid) + " " + name + "\n";
        write(fd_out, reg.c_str(), reg.size());
    }

    print_menu(pid, name);

    string line;
    while (true) {
        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        if (fd_in >= 0) FD_SET(fd_in, &rfds);
        int maxfd = (fd_in >= 0) ? max(STDIN_FILENO, fd_in) : STDIN_FILENO;

        if (select(maxfd + 1, &rfds, nullptr, nullptr, nullptr) < 0) break;

        // Mensajes desde el guardian (ACKs / broadcasts)
        if (fd_in >= 0 && FD_ISSET(fd_in, &rfds)) {
            char buf[1024];
            ssize_t n = read(fd_in, buf, sizeof(buf) - 1);
            if (n > 0) { buf[n] = '\0'; cout << buf << flush; }
        }

        // Entrada del usuario
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            if (!getline(cin, line)) break;  // EOF

            if (line == "/menu") { print_menu(pid, name); continue; }

            if (line == "/leave") {
                string xao = "MSG " + to_string(pid) + " [" + name + "@" + to_string(pid) + "]: [leave]\n";
                (void)write(fd_out, xao.c_str(), xao.size());  // avisar y salir
                break;
            }

            string msg = "MSG " + to_string(pid) + " [" + name + "@" + to_string(pid) + "]: " + line + "\n";
            (void)write(fd_out, msg.c_str(), msg.size());
        }
    }

    if (fd_in >= 0) { close(fd_in); unlink(downlink.c_str()); }
    close(fd_out);
    return 0;
}
