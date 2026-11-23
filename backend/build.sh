#!/bin/bash
set -e

echo "========================================================"
echo "    Building Medico C++ Backend for Linux / Render     "
echo "========================================================"

# Detect MySQL / MariaDB compilation and link flags
if command -v mysql_config >/dev/null 2>&1; then
    MYSQL_FLAGS="$(mysql_config --cflags)"
    MYSQL_LIBS="$(mysql_config --libs)"
elif command -v mariadb_config >/dev/null 2>&1; then
    MYSQL_FLAGS="$(mariadb_config --cflags)"
    MYSQL_LIBS="$(mariadb_config --libs)"
else
    MYSQL_FLAGS="-I/usr/include/mysql -I/usr/include/mariadb"
    MYSQL_LIBS="-lmysqlclient -lz -lzstd -lssl -lcrypto -lresolv -lm"
fi

# Detect cURL flags
if command -v curl-config >/dev/null 2>&1; then
    CURL_FLAGS="$(curl-config --cflags)"
    CURL_LIBS="$(curl-config --libs)"
else
    CURL_FLAGS=""
    CURL_LIBS="-lcurl"
fi

echo "Compiler flags: $MYSQL_FLAGS $CURL_FLAGS"
echo "Linker libs:    $MYSQL_LIBS $CURL_LIBS"

# Compile with memory optimization flags for low-RAM build environments
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
    -pthread

echo "========================================================"
echo " [BUILD SUCCESS] Linux server binary created:"
ls -lh server
echo "========================================================"
