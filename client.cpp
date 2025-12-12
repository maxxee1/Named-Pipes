#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
using namespace std;


/*************  CLIENTE -> SERVIDOR  *************/
static string base   = "/tmp/sochat";
static string pipeSubida = base + "/guardian.in";


/****************  FUNCION ENVIAR MENSAJES  ****************/
static void enviarLinea(int pipeEntrada, const string &s){
    string line = s; if(line.empty() || line.back()!='\n') line.push_back('\n');
    (void)write(pipeEntrada, line.c_str(), line.size());
}


int main(int argc, char** argv) {
    /**********  CONFIGURACION DE ENTORNO  **********/
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.setf(std::ios::unitbuf);
    signal(SIGPIPE, SIG_IGN);
    mkdir(base.c_str(), 0777);


    /****************  NOMBRE CLIENTE CON ASINGACION DE PID  ***************/
    string nombreCliente = (argc>=2 ? argv[1] : string("usuario_") + to_string(getpid()));
    int idFamilia = 0;

    for(int i=2;i<argc;i++) {
        if(string(argv[i])=="--family" && i+1<argc) {
            idFamilia = atoi(argv[i+1]);
            i++;
        }
    }

    int mypid = (int)getpid();

    if(idFamilia == 0) {
        idFamilia = mypid;
    }


    /******  MATAR AL AL HIJO SI MUERE EL PADRE  ******/
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    /******** FIFO DE BAJADA SERVIDOR -> CLIENTE  ********/
    string pipeBajada = base + "/guardian_to_" + to_string(mypid);
    mkfifo(pipeBajada.c_str(), 0666);
    int pipeSalida = open(pipeBajada.c_str(), O_RDONLY | O_NONBLOCK);
    
    if(pipeSalida < 0) {
        perror("[cliente] Error al abrir pipe de ENTRADA (bajada)");
        return 1;
    }

    /*********  BLOQUEAR HASTA QUE EL SERVER LEA  *********/
    int pipeEntrada = open(pipeSubida.c_str(), O_WRONLY);

    if(pipeEntrada < 0) {
        perror("[cliente] Error al abrir pipe de SALIDA (subida)");
        return 1;
    }


    enviarLinea(pipeEntrada, "REGISTER|" + to_string(mypid) + "|" + nombreCliente + "|" + to_string(idFamilia));

    cout << "\n========== MENÚ DEL CLIENTE ==========\n"
     << "PID      : " << mypid << "\n"
     << "Nombre   : " << nombreCliente << "\n\n"
     << "Comandos disponibles:\n"
     << "  • reportar <pid>   -> Reportar proceso\n"
     << "  • dup              -> Duplicar este proceso\n"
     << "  • salir            -> Salir del chat\n"
     << "En cualquier otro caso, escriba su mensaje normalmente.\n"
     << "======================================\n" << endl;

    string inbuf; inbuf.reserve(4096);
    char tmp[1024];


    /*************************  BUCLE PRINCIPAL  *************************************/
    while(true){

        ssize_t n = read(pipeSalida, tmp, sizeof(tmp));
        if(n > 0) {
            inbuf.append(tmp, tmp+n);
            size_t pos;
            while((pos = inbuf.find('\n')) != string::npos) {
                string L = inbuf.substr(0,pos);
                inbuf.erase(0,pos+1);
                cout << L << endl;
            }
        }

        /**************  PARA NO BLOQUAR EL TECLADO  ************/
        fd_set rfds; FD_ZERO(&rfds); FD_SET(STDIN_FILENO, &rfds);
        timeval tv{0, 200000};
        int r = select(STDIN_FILENO+1, &rfds, nullptr, nullptr, &tv);
        if(r > 0 && FD_ISSET(STDIN_FILENO, &rfds)) {
            
            
            /***********  MANEJO DE MENSAJES Y COMANDOS  ***********/
            string mensaje;
            if(!getline(cin, mensaje)) { 
                enviarLinea(pipeEntrada, "LEAVE|" + to_string(mypid) + "|");
                break;
            }

            if(mensaje == "salir") {
                enviarLinea(pipeEntrada, "LEAVE|" + to_string(mypid)+ "|");
                break;

            }
            else if(mensaje.rfind("reportar ",0)==0) {
                string w; int target=0;
                stringstream ss(mensaje);
                ss>>w>>target;
                if(target>0) enviarLinea(pipeEntrada, "REPORT|" + to_string(mypid) + "|" + to_string(target));

            }
            else if(mensaje == "dup") {
                pid_t hijo = fork();
                if(hijo == 0) {
                    string cname = nombreCliente + "_dup";
                    execlp("./client", "./client", cname.c_str(), "--family", to_string(idFamilia).c_str(), (char*)nullptr);
                    perror("[client-child] execlp"); _exit(1);
                }
                else if(hijo > 0) {
                    cout << "[client] duplicado lanzado con PID=" << hijo << endl;
                }
                else {
                    perror("[client] fork");
                }

            }
            else {
                string payload = "[" + nombreCliente + "@" + to_string(mypid) + "]: " + mensaje;
                enviarLinea(pipeEntrada, "MSG|" + to_string(mypid) + "|" + payload);
            }
        }
    }

    close(pipeSalida);
    close(pipeEntrada);
    return 0;
}
