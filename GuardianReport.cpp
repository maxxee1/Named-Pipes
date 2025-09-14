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

    /******  MANEJO DE REPORTES  ******/
    map<int,int> cantReportes;
    string buf;
    buf.reserve(4096);
    char tmp[1024];

    auto procesarLinea = [&](string &acumulador) {
        size_t posicion;

        while((posicion = acumulador.find('\n')) != string::npos) {
            string linea = acumulador.substr(0,posicion);
            acumulador.erase(0,posicion+1);

            // Esperamos: REPORT|rep|tar
            if(linea.rfind("REPORT|",0)==0) {
                vector<string> partes;
                string token;
                stringstream flujo(linea);

                while(getline(flujo,token,'|')) partes.push_back(token);

                if(partes.size()==3) {
                    int objetivo = atoi(partes[2].c_str());

                    if(++cantReportes[objetivo] >= 10) {
                        string ordenKill = "KILL|guardian|" + to_string(objetivo) + "\n";
                        (void)write(salidaServidor, ordenKill.c_str(), ordenKill.size());
                        cantReportes.erase(objetivo);
                    }
                }
            }
        }
    };

    while(true) {
        ssize_t n = read(entradaServidor, tmp, sizeof(tmp));
        if(n > 0) {
            buf.append(tmp, tmp+n);
            procesarLinea(buf);
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
