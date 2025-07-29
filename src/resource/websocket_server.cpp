#include "resource/websocket_server.h"
#include "spdlog/spdlog.h"

WebSocketServer::WebSocketServer() {
    // Constructor
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start(int port) {
    setup_websocket_route();
    
    server_thread_ = std::thread([this, port]() {
        spdlog::info("WebSocket server starting on port {}", port);
        if (!svr_.listen("0.0.0.0", port)) {
            spdlog::error("Failed to start WebSocket server on port {}", port);
        }
    });
}

void WebSocketServer::stop() {
    if (svr_.is_running()) {
        svr_.stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    spdlog::info("WebSocket server stopped.");
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::info("Broadcasting message to {} clients: {}", connections_.size(), message);
    for (auto conn : connections_) {
        conn->send(message);
    }
}

void WebSocketServer::setup_websocket_route() {
    svr_.set_on_open_handler([this](Connection conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.insert(conn);
        spdlog::info("WebSocket connection opened. Total clients: {}", connections_.size());
    });

    svr_.set_on_close_handler([this](Connection conn, int, const std::string &) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(conn);
        spdlog::info("WebSocket connection closed. Total clients: {}", connections_.size());
    });

    svr_.set_on_message_handler([this](Connection conn, const std::string& msg) {
        spdlog::info("Received message: {}", msg);
        // Echo the message back to the client
        conn->send("echo: " + msg);
    });
} 