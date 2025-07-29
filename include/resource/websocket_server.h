#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <thread>
#include <mutex>
#include <functional>

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::connection_hdl connection_hdl;

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    void start(int port);
    void stop();
    void broadcast(const std::string& message);
    
    // Callback setters
    void set_on_open_handler(std::function<void(connection_hdl)> handler);
    void set_on_close_handler(std::function<void(connection_hdl)> handler);
    void set_on_message_handler(std::function<void(connection_hdl, server::message_ptr)> handler);

private:
    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, server::message_ptr msg);
    
    server endpoint_;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    std::mutex mutex_;
    std::thread server_thread_;
    bool running_;
    
    // User-defined callbacks
    std::function<void(connection_hdl)> user_on_open_;
    std::function<void(connection_hdl)> user_on_close_;
    std::function<void(connection_hdl, server::message_ptr)> user_on_message_;
};

#endif // WEBSOCKET_SERVER_H 