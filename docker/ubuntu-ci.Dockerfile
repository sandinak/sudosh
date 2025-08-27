# Local Ubuntu CI runner for sudosh matrix
# Mirrors the GitHub Actions ubuntu-22.04 environment for builds
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    clang \
    libpam0g-dev \
    cppcheck \
    valgrind \
    gcovr \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
COPY . /work

# Default command prints help
CMD ["bash", "-lc", "echo 'Use run_ubuntu_matrix.sh to run builds/tests' && ls -la"]

