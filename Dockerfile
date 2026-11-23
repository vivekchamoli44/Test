# Multi-stage Dockerfile for Deploying Medico C++ Backend on Render
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install g++ compiler and MySQL & cURL libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    libmysqlclient-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source repository
COPY . .

# Compile C++ backend for Linux with MySQL and cURL support
RUN g++ -std=c++17 -O2 \
    -Ibackend/include \
    backend/src/main.cpp \
    -o server \
    -lmysqlclient -lcurl -lpthread

# Production runtime image
FROM ubuntu:22.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libmysqlclient21 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled executable and frontend assets
COPY --from=builder /build/server /app/server
COPY frontend /app/frontend
COPY backend/.env.example /app/.env

# Render automatically sets the PORT environment variable
ENV PORT=10000
EXPOSE 10000

CMD ["./server"]
