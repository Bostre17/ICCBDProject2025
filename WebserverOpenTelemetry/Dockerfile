# Dockerfile
FROM ubuntu:22.04

# Installa dipendenze
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Crea directory di lavoro
WORKDIR /app

# Crea directory include e scarica la libreria httplib
RUN mkdir -p include
RUN git clone https://github.com/yhirose/cpp-httplib.git && \
    cp cpp-httplib/httplib.h include/ && \
    rm -rf cpp-httplib

# Copia i file di progetto
COPY CMakeLists.txt .
COPY WebServer.cpp .

# Compila il progetto
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Esponi la porta
EXPOSE 8080

# Comando di avvio
CMD ["/app/build/webserver"]