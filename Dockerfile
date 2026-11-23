# Production Dockerfile for Medico C++ Backend on Render
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install g++ compiler, MySQL & MariaDB client development packages, cURL, OpenSSL, zstd, and build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    default-libmysqlclient-dev \
    libmysqlclient-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    libzstd-dev \
    zlib1g-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy all repository files (backend, frontend)
COPY . .

# Compile C++ backend:
# Uses mysql_config and curl-config to supply complete system libraries (zstd, zlib, ssl, crypto, resolv, dl)
# Uses -Os and GCC memory tuning parameters to stay safely within Render Free RAM limits
RUN sh -c '\
    set -e; \
    MYSQL_FLAGS="$(mysql_config --cflags 2>/dev/null || echo -I/usr/include/mysql)"; \
    MYSQL_LIBS="$(mysql_config --libs 2>/dev/null || echo -lmysqlclient -lz -lzstd -lssl -lcrypto -lresolv -lm)"; \
    CURL_FLAGS="$(curl-config --cflags 2>/dev/null || echo)"; \
    CURL_LIBS="$(curl-config --libs 2>/dev/null || echo -lcurl)"; \
    echo "=== Building Medico C++ Backend ==="; \
    echo "MySQL Flags: $MYSQL_FLAGS"; \
    echo "MySQL Libs:  $MYSQL_LIBS"; \
    echo "cURL Libs:   $CURL_LIBS"; \
    g++ -std=c++17 -Os \
        --param ggc-min-expand=10 \
        --param ggc-min-heapsize=32768 \
        -fno-var-tracking-assignments \
        -Ibackend/include \
        -Ibackend/src \
        $MYSQL_FLAGS \
        $CURL_FLAGS \
        backend/src/main.cpp \
        -o server \
        $MYSQL_LIBS \
        $CURL_LIBS \
        -pthread; \
    echo "=== Build Complete: server binary successfully created ==="; \
    ls -lh server \
    '

# Render automatically injects the PORT environment variable (default 10000)
ENV PORT=10000
EXPOSE 10000

CMD ["./server"]
