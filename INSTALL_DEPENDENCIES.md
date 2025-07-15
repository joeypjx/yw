# Dependencies Installation Guide

## TDengine Client Library

### macOS (Homebrew)

```bash
# Install TDengine client
brew install tdengine

# Or download from official site
curl -L https://github.com/taosdata/TDengine/releases/download/ver-3.0.0.0/TDengine-client-3.0.0.0-Darwin-x64.tar.gz -o tdengine-client.tar.gz
tar -xzf tdengine-client.tar.gz
cd TDengine-client-3.0.0.0
sudo ./install_client.sh
```

### Linux (Ubuntu/Debian)

```bash
# Download and install TDengine client
wget https://github.com/taosdata/TDengine/releases/download/ver-3.0.0.0/TDengine-client-3.0.0.0-Linux-x64.tar.gz
tar -xzf TDengine-client-3.0.0.0-Linux-x64.tar.gz
cd TDengine-client-3.0.0.0
sudo ./install_client.sh
```

### Verification

After installation, verify the installation:

```bash
# Check if library exists
ls /usr/local/lib/libtaos.* || ls /opt/homebrew/lib/libtaos.*

# Check if header exists
ls /usr/local/include/taos.h || ls /opt/homebrew/include/taos.h
```

## Build Instructions

After installing TDengine client library:

```bash
mkdir build
cd build
cmake ..
make
```

## Run Tests

```bash
# Make sure TDengine server is running first
cd build
make test
```

## TDengine Server Setup

If you need to run TDengine server locally:

```bash
# macOS
brew install tdengine-server

# Linux
sudo systemctl start taosd
sudo systemctl enable taosd
```

Connect to TDengine server:
```bash
taos -h 127.0.0.1 -u test -p
# Password: HZ715Net
```