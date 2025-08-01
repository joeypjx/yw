#include "resource/websocket_server.h"
#include "spdlog/spdlog.h"

// Static constants for ping/pong timing
const std::chrono::seconds WebSocketServer::PING_INTERVAL(30);  // Send ping every 30 seconds
const std::chrono::seconds WebSocketServer::PONG_TIMEOUT(10);   // Wait 10 seconds for pong response

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
    
    // Set ping/pong handlers
    endpoint_.set_ping_handler(std::bind(&WebSocketServer::on_ping, this, std::placeholders::_1, std::placeholders::_2));
    endpoint_.set_pong_handler(std::bind(&WebSocketServer::on_pong, this, std::placeholders::_1, std::placeholders::_2));
    endpoint_.set_pong_timeout_handler(std::bind(&WebSocketServer::on_pong_timeout, this, std::placeholders::_1, std::placeholders::_2));
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start(int port) {
    try {
        // Set socket options for faster resource cleanup
        endpoint_.set_reuse_addr(true);
        
        endpoint_.listen(port);
        endpoint_.start_accept();
        
        running_ = true;
        server_thread_ = std::thread([this]() {
            spdlog::info("WebSocket server starting on port");
            endpoint_.run();
        });
        
        // Start ping timer thread
        ping_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(PING_INTERVAL);
                if (running_) {
                    ping_timer_callback();
                }
            }
        });
        
        spdlog::info("WebSocket server started successfully");
    } catch (websocketpp::exception const& e) {
        spdlog::error("Failed to start WebSocket server: {}", e.what());
    }
}

void WebSocketServer::stop() {
    if (running_) {
        running_ = false;
        
        // First, explicitly close all active connections
        {
            std::lock_guard<std::mutex> lock(mutex_);
            spdlog::info("Closing {} active connections...", connections_.size());
            
            for (auto hdl : connections_) {
                try {
                    endpoint_.close(hdl, websocketpp::close::status::going_away, "Server shutting down");
                } catch (websocketpp::exception const& e) {
                    spdlog::error("Error closing connection during shutdown: {}", e.what());
                }
            }
        }
        
        // Stop accepting new connections
        endpoint_.stop_listening();
        
        // Give connections a moment to close gracefully
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop the endpoint
        endpoint_.stop();
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        if (ping_thread_.joinable()) {
            ping_thread_.join();
        }
        
        // Clear all tracking data
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.clear();
            last_pong_time_.clear();
            ping_pending_.clear();
        }
        
        spdlog::info("WebSocket server stopped and all resources cleaned up.");
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
    
    // Initialize ping/pong tracking for this connection
    last_pong_time_[hdl] = std::chrono::steady_clock::now();
    ping_pending_[hdl] = false;
    
    spdlog::info("WebSocket connection opened. Total clients: {}", connections_.size());
    
    if (user_on_open_) {
        user_on_open_(hdl);
    }
}

void WebSocketServer::on_close(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(hdl);
    
    // Clean up ping/pong tracking for this connection
    last_pong_time_.erase(hdl);
    ping_pending_.erase(hdl);
    
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

bool WebSocketServer::on_ping(connection_hdl hdl, std::string payload) {
    spdlog::debug("Received ping from client, payload: {}", payload);
    // Return true to automatically send pong response
    return true;
}

void WebSocketServer::on_pong(connection_hdl hdl, std::string payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::debug("Received pong from client, payload: {}", payload);
    
    // Update last pong time and clear pending ping flag
    last_pong_time_[hdl] = std::chrono::steady_clock::now();
    ping_pending_[hdl] = false;
}

void WebSocketServer::on_pong_timeout(connection_hdl hdl, std::string payload) {
    spdlog::warn("Pong timeout from client, closing connection. Payload: {}", payload);
    
    try {
        // Close the connection due to pong timeout
        endpoint_.close(hdl, websocketpp::close::status::protocol_error, "Pong timeout");
    } catch (websocketpp::exception const& e) {
        spdlog::error("Error closing connection on pong timeout: {}", e.what());
    }
}

void WebSocketServer::send_ping_to_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connections_.empty()) {
        return;
    }
    
    spdlog::debug("Sending ping to {} clients", connections_.size());
    
    for (auto hdl : connections_) {
        try {
            // Mark ping as pending for this connection
            ping_pending_[hdl] = true;
            
            // Send ping with current timestamp as payload
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            std::string payload = std::to_string(now);
            
            endpoint_.ping(hdl, payload);
        } catch (websocketpp::exception const& e) {
            spdlog::error("Error sending ping: {}", e.what());
        }
    }
}

void WebSocketServer::ping_timer_callback() {
    check_connection_health();
    send_ping_to_all();
}

void WebSocketServer::check_connection_health() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    std::vector<connection_hdl> connections_to_close;
    
    for (auto hdl : connections_) {
        // Check if pong timeout exceeded
        auto last_pong_it = last_pong_time_.find(hdl);
        auto ping_pending_it = ping_pending_.find(hdl);
        
        if (last_pong_it != last_pong_time_.end() && ping_pending_it != ping_pending_.end()) {
            auto time_since_last_pong = now - last_pong_it->second;
            
            // If ping is pending and timeout exceeded, mark for closure
            if (ping_pending_it->second && time_since_last_pong > (PING_INTERVAL + PONG_TIMEOUT)) {
                spdlog::warn("Connection health check failed - no pong received within timeout");
                connections_to_close.push_back(hdl);
            }
        }
    }
    
    // Close unhealthy connections (do this outside the main loop to avoid iterator invalidation)
    for (auto hdl : connections_to_close) {
        try {
            spdlog::info("Closing unhealthy connection due to ping/pong timeout");
            endpoint_.close(hdl, websocketpp::close::status::protocol_error, "Health check failed");
        } catch (websocketpp::exception const& e) {
            spdlog::error("Error closing unhealthy connection: {}", e.what());
        }
    }
} 