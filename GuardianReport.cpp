#include <bits/stdc++.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
using namespace std;

int main(){
    /******  CONFIGURACION DE ENTORNO  ******/
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    
    const string base = "/tmp/sochat";

    /*********  SERVIDOR -> GUARDIAN  *********/
    const string desdeServer = base + "/srv_to_guard.fifo";

    /*********  GUARDIAN -> SERVIDOR  *********/
    const string palServer   = base + "/guard_to_srv.fifo";
    mkdir(base.c_str(), 0777);
    mkfifo(desdeServer.c_str(), 0666);
    mkfifo(palServer.c_str(),   0666);

    int entradaServidor  = open(desdeServer.c_str(), O_RDONLY);

    if(entradaServidor  < 0) {
        perror("[guardian] open from_srv");
        return 1;
    }
    int salidaServidor = open(palServer.c_str(), O_WRONLY);
    
    if(salidaServidor < 0) {
        perror("[guardian] open to_srv");
        return 1;
    }

    map<int,int> cantReportes;
    string buf;
    buf.reserve(4096);
    char tmp[1024];

    auto flush_lines = [&](string &acc) {
        size_t pos;

        while((pos = acc.find('\n')) != string::npos) {
            string linea = acc.substr(0,pos);
            acc.erase(0,pos+1);

            // Esperamos: REPORT|rep|tar
            if(linea.rfind("REPORT|",0)==0) {
                vector<string> parts; string t; stringstream ss(linea);

                while(getline(ss,t,'|')) parts.push_back(t);

                if(parts.size()==3) {
                    int objetivo = atoi(parts[2].c_str());

                    if(++cantReportes[objetivo] > 9) {
                        string killMsg = "KILL|guardian|" + to_string(objetivo) + "\n";
                        (void)write(salidaServidor, killMsg.c_str(), killMsg.size());
                        cantReportes.erase(objetivo); // reset para ese pid
                    }
                }
            }
        }
    };

    while(true) {
        ssize_t n = read(entradaServidor, tmp, sizeof(tmp));
        if(n > 0) {
            buf.append(tmp, tmp+n);
            flush_lines(buf);
        }
        else if(n == 0) {
            close(entradaServidor);
            entradaServidor = open(desdeServer.c_str(), O_RDONLY);

            if(entradaServidor < 0) {
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

    close(entradaServidor);
    close(salidaServidor);
    return 0;
}
