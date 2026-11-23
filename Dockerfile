# Production Dockerfile for Medico C++ Backend on Render
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install g++ compiler and MySQL & cURL libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    default-libmysqlclient-dev \
    libmysqlclient-dev \
    libcurl4-openssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy all repository files (backend, frontend)
COPY . .

# Compile C++ backend for Linux with MySQL, cURL, and pthread
RUN g++ -std=c++17 -O2 \
    -Ibackend/include \
    -Ibackend/src \
    -I/usr/include/mysql \
    backend/src/main.cpp \
    -o server \
    -L/usr/lib/x86_64-linux-gnu \
    -lmysqlclient -lcurl -lpthread

# Render automatically injects the PORT environment variable (default 10000)
ENV PORT=10000
EXPOSE 10000

CMD ["./server"]

