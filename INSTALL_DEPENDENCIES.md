# Dependencies Installation Guide

## MySQL Client Library

### macOS (Homebrew)

```bash
# Install MySQL client
brew install mysql-client

# Or install full MySQL server
brew install mysql
```

### Linux (Ubuntu/Debian)

```bash
# Install MySQL client library
sudo apt-get update
sudo apt-get install libmysqlclient-dev

# Or install full MySQL server
sudo apt-get install mysql-server mysql-client
```

### Verification

After installation, verify the installation:

```bash
# Check if library exists
ls /usr/local/lib/libmysqlclient.* || ls /opt/homebrew/lib/libmysqlclient.*

# Check if header exists
ls /usr/local/include/mysql/mysql.h || ls /opt/homebrew/include/mysql/mysql.h
```

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

After installing MySQL client library and TDengine client library:

```bash
mkdir build
cd build
cmake ..
make
```

## Run Tests

```bash
# Make sure both MySQL and TDengine servers are running first
cd build
make test
```

## MySQL Server Setup

If you need to run MySQL server locally:

```bash
# macOS
brew services start mysql

# Linux
sudo systemctl start mysql
sudo systemctl enable mysql
```

Connect to MySQL server:
```bash
mysql -h 127.0.0.1 -u test -p
# Password: HZ715Net
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