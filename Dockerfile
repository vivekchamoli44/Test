# Production Dockerfile for Medico C++ Backend on Render
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install g++ compiler, MySQL client, cURL, OpenSSL, and compression libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    default-libmysqlclient-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    zlib1g-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy all repository files (backend, frontend)
COPY . .

# Compile C++ backend with memory optimization flags to prevent OOM on Render Free tier:
# -Os: optimize for size (avoids massive template expansion in Crow/Asio)
# --param ggc-min-expand=10 --param ggc-min-heapsize=32768: aggressive garbage collection of AST nodes
# -fno-var-tracking-assignments: prevents huge RAM consumption during code generation
# -pthread: required for POSIX threads / std::thread on Linux
RUN g++ -std=c++17 -Os \
    --param ggc-min-expand=10 \
    --param ggc-min-heapsize=32768 \
    -fno-var-tracking-assignments \
    -Ibackend/include \
    -Ibackend/src \
    -I/usr/include/mysql \
    -I/usr/include/mariadb \
    backend/src/main.cpp \
    -o server \
    -lmysqlclient -lcurl -lssl -lcrypto -ldl -lz -pthread || \
    g++ -std=c++17 -Os \
    --param ggc-min-expand=10 \
    --param ggc-min-heapsize=32768 \
    -fno-var-tracking-assignments \
    -Ibackend/include \
    -Ibackend/src \
    -I/usr/include/mysql \
    -I/usr/include/mariadb \
    backend/src/main.cpp \
    -o server \
    -lmariadb -lcurl -lssl -lcrypto -ldl -lz -pthread

# Render automatically injects the PORT environment variable (default 10000)
ENV PORT=10000
EXPOSE 10000

CMD ["./server"]
