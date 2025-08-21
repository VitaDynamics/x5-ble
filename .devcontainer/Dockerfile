FROM arm64v8/ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config \
    libbluetooth-dev libglib2.0-dev ca-certificates \
    gdb valgrind \
    openssh-server \
    && update-ca-certificates

# Install gattlib
WORKDIR /opt
RUN git clone --depth=1 https://github.com/labapart/gattlib.git
WORKDIR /opt/gattlib/build
RUN cmake .. -DGATTLIB_SHARED_LIB=OFF -DGATTLIB_ENABLE_EXAMPLES=OFF
RUN make -j && make install
RUN ldconfig

# Create vscode user
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && apt-get update \
    && apt-get install -y sudo \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Set up SSH for debugging
RUN mkdir -p /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN echo 'PermitRootLogin yes' >> /etc/ssh/sshd_config
RUN echo 'PasswordAuthentication yes' >> /etc/ssh/sshd_config
RUN echo 'PermitEmptyPasswords yes' >> /etc/ssh/sshd_config

WORKDIR /ws