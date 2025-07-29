#include "resource/websocket_server.h"
#include "spdlog/spdlog.h"

WebSocketServer::WebSocketServer() : running_(false) {
    // Set logging level
    endpoint_.set_access_channels(websocketpp::log::alevel::all);
    endpoint_.clear_access_channels(websocketpp::log::alevel::frame_payload);
    
    // Initialize ASIO
    endpoint_.init_asio();
    
    // Set handlers
    endpoint_.set_open_handler(std::bind(&WebSocketServer::on_open, this, std::placeholders::_1));
    endpoint_.set_close_handler(std::bind(&WebSocketServer::on_close, this, std::placeholders::_1));
    endpoint_.set_message_handler(std::bind(&WebSocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start(int port) {
    try {
        endpoint_.listen(port);
        endpoint_.start_accept();
        
        running_ = true;
        server_thread_ = std::thread([this]() {
            spdlog::info("WebSocket server starting on port");
            endpoint_.run();
        });
        
        spdlog::info("WebSocket server started successfully");
    } catch (websocketpp::exception const& e) {
        spdlog::error("Failed to start WebSocket server: {}", e.what());
    }
}

void WebSocketServer::stop() {
    if (running_) {
        running_ = false;
        endpoint_.stop();
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        spdlog::info("WebSocket server stopped.");
    }
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::info("Broadcasting message to {} clients: {}", connections_.size(), message);
    
    for (auto hdl : connections_) {
        try {
            endpoint_.send(hdl, message, websocketpp::frame::opcode::text);
        } catch (websocketpp::exception const& e) {
            spdlog::error("Error sending message: {}", e.what());
        }
    }
}

void WebSocketServer::set_on_open_handler(std::function<void(connection_hdl)> handler) {
    user_on_open_ = handler;
}

void WebSocketServer::set_on_close_handler(std::function<void(connection_hdl)> handler) {
    user_on_close_ = handler;
}

void WebSocketServer::set_on_message_handler(std::function<void(connection_hdl, server::message_ptr)> handler) {
    user_on_message_ = handler;
}

void WebSocketServer::on_open(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.insert(hdl);
    spdlog::info("WebSocket connection opened. Total clients: {}", connections_.size());
    
    if (user_on_open_) {
        user_on_open_(hdl);
    }
}

void WebSocketServer::on_close(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(hdl);
    spdlog::info("WebSocket connection closed. Total clients: {}", connections_.size());
    
    if (user_on_close_) {
        user_on_close_(hdl);
    }
}

void WebSocketServer::on_message(connection_hdl hdl, server::message_ptr msg) {
    spdlog::info("Received message: {}", msg->get_payload());
    
    if (user_on_message_) {
        user_on_message_(hdl, msg);
    } else {
        // Default echo behavior
        try {
            endpoint_.send(hdl, "echo: " + msg->get_payload(), websocketpp::frame::opcode::text);
        } catch (websocketpp::exception const& e) {
            spdlog::error("Error echoing message: {}", e.what());
        }
    }
} 