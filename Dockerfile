# Production Dockerfile for Medico C++ Backend on Render
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install g++ compiler, MySQL client, cURL, OpenSSL, zstd, and build tools
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

# Compile C++ backend via dedicated build script
RUN chmod +x backend/build.sh && bash backend/build.sh

# Render automatically injects the PORT environment variable (default 10000)
ENV PORT=10000
EXPOSE 10000

CMD ["./server"]
