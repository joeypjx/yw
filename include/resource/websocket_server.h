#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include "httplib.h"
#include <mutex>
#include <set>
#include <thread>

// Define a type alias for the connection pointer for clarity
using Connection = httplib::WebSocketSession*;

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    void start(int port);
    void stop();
    void broadcast(const std::string& message);

private:
    void setup_websocket_route();

    httplib::Server svr_;
    std::set<Connection> connections_;
    std::mutex mutex_;
    std::thread server_thread_;
};

#endif // WEBSOCKET_SERVER_H 