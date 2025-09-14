# README – Tarea 1: Sistemas Operativos  
Autores: **Solorza Maximiliano** y **Soto Benjamín**

---

## 📂 Organización de archivos
Todos los archivos fuente deben estar contenidos en una carpeta de trabajo.  
Ejemplo:  

```
Solorza_Maximiliano_Soto_Benjamin/
│── client.cpp
│── server.cpp
│── GuardianReport.cpp
│── Makefile
│── README.md
```

---

## ⚙️ Compilación Manual
Abrir una terminal y ejecutar:

```bash
cd /path_donde_tenga_la_carpeta/SolorzaMaximiliano_SotoBenjamin
g++ client.cpp -o client
g++ server.cpp -o server
g++ GuardianReport.cpp -o guardian
```

Esto generará tres binarios ejecutables:  
- `client`  
- `server`  
- `guardian`  


## ⭐ Compilación Makefile

Si se prefiere, se puede usar el **Makefile** incluido para compilar automáticamente todos los binarios con un solo comando:

```bash
make
```
Esto genera server, guardian y client en la misma carpeta.

Para limpiar los binarios:
```bash
make clean
```

---

## ▶️ Ejecución
Cada proceso se debe ejecutar en una **terminal distinta**. El orden sugerido es:

1. **Proceso central (server):**
   ```bash
   ./server
   ```

2. **Proceso de reportes (guardian):**
   ```bash
   ./guardian
   ```

3. **Clientes (tantos como se desee):**
   ```bash
   ./client "NombreCliente"
   ```
   - Si no se especifica un nombre, se asignará uno automáticamente en base al PID.  
   - Cada cliente muestra un menú con comandos disponibles:
     - `mensaje` → se envía al muro y lo reciben todos los clientes conectados.
     - `salir` → desconecta al cliente del chat.
     - `reportar <pid>` → reporta a un proceso por su PID.
     - `dup` → crea una copia del cliente actual (hijo idéntico).

---

## 💬 Funcionamiento general
- El **servidor central** (server) actúa como orquestador:  
  recibe todos los mensajes de los clientes y los distribuye al “muro” (difusión a todos los clientes conectados).  
- El **guardian** lleva el conteo de reportes por PID. Si un cliente acumula **10 reportes**, se envía la orden al servidor para **expulsar** al proceso y cerrarlo.  
- Los **clientes** se comunican con el servidor usando **FIFOs nombradas** en `/tmp/sochat`. Cada cliente tiene un canal único para recibir mensajes.

---

## 🔄 Particularidad al duplicar clientes
- Con el comando `dup`, un cliente se clona (`fork` + `exec`) y ambos procesos quedan conectados al servidor.  
- Como **comparten la misma FIFO de escritura hacia el server**, al enviar un mensaje solo uno de ellos logra mandarlo (dependiendo de cuál gana la escritura en ese momento).  
- Sin embargo, al **recibir**, ambos procesos leen los mensajes entrantes y, por lo tanto, verán todos los mensajes duplicados (uno para el padre y otro para el hijo).  
- Este comportamiento es **esperado**, ya que ambos procesos actúan como clientes independientes, pero con un canal compartido de subida.

---

## 📖 Ejemplo de sesión
1. Iniciar `server` en una terminal.  
2. Iniciar `guardian` en otra.  
3. Abrir tres clientes en terminales distintas:  
   ```bash
   ./client "Alice"
   ./client "Bob"
   ./client "Carlos"
   ```
4. Alice envía: `hola a todos` → Bob y Carlos reciben el mensaje y el muro.  
5. Bob reporta a Carlos diez veces: `reportar <pidCarlos>` → guardian detecta los 10 reportes y envía orden de KILL → Carlos es expulsado automáticamente.  
6. Alice escribe `dup` → ahora hay Alice y Alice_dup conectados como dos clientes distintos.
7. Bob envía: `qienes estan?` → Alice recibe 2 veces ese mensaje en su terminal ya que se duplico
8. La termina de Alice responde: `yo` → Debido a que comparte FIFO con su clon uno de los 2 procesos al azar es el que manda ese mensaje

---

## 📌 Resumen
Este trabajo implementa un **chat comunitario en C++** con la siguiente arquitectura:
- **Un proceso central (server)**: maneja todos los logs y difunde mensajes.  
- **Múltiples clientes independientes**: pueden mandar mensajes, salir, duplicarse o reportar.  
- **Un proceso guardian**: controla los reportes y expulsa a procesos conflictivos.  

La comunicación se realiza exclusivamente por **pipes nombrados (FIFOs)** bajo `/tmp/sochat`.  
