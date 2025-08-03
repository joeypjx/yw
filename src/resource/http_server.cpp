#include "http_server.h"
#include "node_model.h"
#include "log_manager.h"
#include "json.hpp"
#include <iostream>
#include <regex>
#include <tuple>
#include <sstream>

using json = nlohmann::json;

const char *get_web_page_html()
{
    return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Alarm Management</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 2em; background-color: #f4f6f8; color: #333; }
        h1, h2 { color: #1a253c; }
        #container { max-width: 1200px; margin: 0 auto; background: white; padding: 2em; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        table { width: 100%; border-collapse: collapse; margin-bottom: 1.5em; }
        th, td { border: 1px solid #ddd; padding: 12px; text-align: left; vertical-align: top; }
        th { background-color: #f2f2f2; font-weight: 600; }
        tr:nth-child(even) { background-color: #f9f9f9; }
        form { display: grid; grid-template-columns: 150px 1fr; gap: 15px; margin-top: 1em; padding: 1.5em; border: 1px solid #ddd; border-radius: 5px; background-color: #fafafa; }
        form label { font-weight: 600; text-align: right; padding-top: 8px; }
        input[type="text"], textarea { width: 100%; padding: 8px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
        textarea { resize: vertical; }
        .form-buttons { grid-column: 2; display: flex; gap: 10px; }
        button { cursor: pointer; padding: 10px 15px; border: none; border-radius: 4px; font-size: 1em; font-weight: 500; }
        button[type="submit"] { background-color: #28a745; color: white; }
        #cancel-edit, .refresh-btn { background-color: #6c757d; color: white; }
        .refresh-btn:disabled { background-color: #adb5bd; cursor: not-allowed; }
        .refresh-btn.refreshing { background-color: #ffc107; color: #212529; }
        .actions-cell button { margin-right: 5px; padding: 5px 10px; font-size: 0.9em; }
        .edit-btn { background-color: #007bff; color: white; }
        .delete-btn { background-color: #dc3545; color: white; }
        pre { white-space: pre-wrap; word-wrap: break-word; background: #eee; padding: 5px; border-radius: 3px; }
        .section-header { display: flex; justify-content: space-between; align-items: center; }
        .status-online { color: #28a745; font-weight: bold; }
        .status-offline { color: #dc3545; font-weight: bold; }
        .status-warning { color: #ffc107; font-weight: bold; }
        .last-update { position: fixed; top: 10px; right: 20px; background: rgba(0,0,0,0.7); color: white; padding: 5px 10px; border-radius: 3px; font-size: 0.8em; z-index: 1000; }
        .websocket-status { position: fixed; top: 10px; left: 20px; background: rgba(0,0,0,0.7); color: white; padding: 5px 10px; border-radius: 3px; font-size: 0.8em; z-index: 1000; }
        .ws-connected { background: rgba(40,167,69,0.8) !important; }
        .ws-disconnected { background: rgba(220,53,69,0.8) !important; }
        .notification { position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); background: #fff; border: 2px solid #007bff; border-radius: 8px; padding: 20px; box-shadow: 0 4px 20px rgba(0,0,0,0.3); z-index: 2000; max-width: 400px; display: none; }
        .notification h3 { margin-top: 0; color: #007bff; }
        .notification button { margin-top: 15px; background: #007bff; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; }
        .notification button:hover { background: #0056b3; }
        .notification-overlay { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); z-index: 1999; display: none; }
    </style>
</head>
<body>
    <div id="container">
        <div id="last-update" class="last-update">Last update: Never</div>
        <div id="websocket-status" class="websocket-status ws-disconnected">WebSocket: Disconnected</div>
        <h1>System Management</h1>

        <!-- Node Metrics Section -->
        <div class="section-header">
            <h2>Node Metrics</h2>
            <button id="refresh-metrics" class="refresh-btn">Refresh Metrics</button>
        </div>
        <table id="metrics-table">
            <thead>
                <tr>
                    <th>Host IP</th>
                    <th>CPU Usage</th>
                    <th>CPU Load</th>
                    <th>Memory Usage</th>
                    <th>Memory Used/Total</th>
                    <th>Disk Usage</th>
                    <th>Network (RX/TX)</th>
                    <th>GPU Count</th>
                    <th>Last Update</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
        
        <!-- Metrics Pagination Controls -->
        <div id="metrics-pagination" style="margin-top: 20px; text-align: center; display: none;">
            <button id="metrics-prev-page" class="refresh-btn">Previous</button>
            <span id="metrics-page-info" style="margin: 0 20px;">Page 1 of 1</span>
            <button id="metrics-next-page" class="refresh-btn">Next</button>
            <span style="margin-left: 20px;">
                Page size: 
                <select id="metrics-page-size-select">
                    <option value="10">10</option>
                    <option value="20" selected>20</option>
                    <option value="50">50</option>
                    <option value="100">100</option>
                </select>
            </span>
        </div>

        <!-- Alarm Rules Section -->
        <div class="section-header">
            <h2>Alarm Rules</h2>
            <button id="refresh-rules" class="refresh-btn">Refresh Rules</button>
        </div>
        <table id="rules-table">
            <thead>
                <tr>
                    <th>Name</th>
                    <th>Expression</th>
                    <th>For</th>
                    <th>Severity</th>
                    <th>Summary</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
        
        <!-- Rules Pagination Controls -->
        <div id="rules-pagination" style="margin-top: 20px; text-align: center; display: none;">
            <button id="rules-prev-page" class="refresh-btn">Previous</button>
            <span id="rules-page-info" style="margin: 0 20px;">Page 1 of 1</span>
            <button id="rules-next-page" class="refresh-btn">Next</button>
            <span style="margin-left: 20px;">
                Page size: 
                <select id="rules-page-size-select">
                    <option value="10">10</option>
                    <option value="20" selected>20</option>
                    <option value="50">50</option>
                    <option value="100">100</option>
                </select>
            </span>
        </div>
        
        <h3>Add / Edit Rule</h3>
        <form id="rule-form">
            <input type="hidden" id="rule-id">
            
            <label for="alert_name">Alert Name:</label>
            <input type="text" id="alert_name" required>
            
            <label for="expression">Expression (JSON):</label>
            <textarea id="expression" rows="5" required></textarea>
            
            <label for="for_duration">For:</label>
            <input type="text" id="for_duration" value="1m" required>
            
            <label for="severity">Severity:</label>
            <input type="text" id="severity" value="warning" required>
            
            <label for="summary">Summary:</label>
            <input type="text" id="summary" required>
            
            <label for="description">Description:</label>
            <input type="text" id="description" required>
            
            <label for="alert_type">Alert Type:</label>
            <input type="text" id="alert_type" value="metric" required>
            
            <label></label>
            <div class="form-buttons">
                <button type="submit">Save Rule</button>
                <button type="button" id="cancel-edit">Cancel</button>
            </div>
        </form>

        <!-- Alarm Events Section -->
        <div class="section-header">
            <h2>Alarm Events</h2>
            <button id="refresh-events" class="refresh-btn">Refresh Events</button>
        </div>
        <table id="events-table">
            <thead>
                <tr>
                    <th>Status</th>
                    <th>Labels</th>
                    <th>Annotations</th>
                    <th>Starts At</th>
                    <th>Ends At</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
        
        <!-- Pagination Controls -->
        <div id="events-pagination" style="margin-top: 20px; text-align: center; display: none;">
            <button id="prev-page" class="refresh-btn">Previous</button>
            <span id="page-info" style="margin: 0 20px;">Page 1 of 1</span>
            <button id="next-page" class="refresh-btn">Next</button>
            <span style="margin-left: 20px;">
                Page size: 
                <select id="page-size-select">
                    <option value="10">10</option>
                    <option value="20" selected>20</option>
                    <option value="50">50</option>
                    <option value="100">100</option>
                </select>
            </span>
        </div>
    </div>

    <!-- Notification popup -->
    <div id="notification-overlay" class="notification-overlay"></div>
    <div id="notification" class="notification">
        <h3>WebSocket Notification</h3>
        <div id="notification-content">Message from server...</div>
        <button onclick="closeNotification()">Close</button>
    </div>

    <script>
    document.addEventListener('DOMContentLoaded', () => {
        const metricsTableBody = document.querySelector('#metrics-table tbody');
        const rulesTableBody = document.querySelector('#rules-table tbody');
        const eventsTableBody = document.querySelector('#events-table tbody');
        const ruleForm = document.getElementById('rule-form');
        const ruleIdInput = document.getElementById('rule-id');
        const cancelEditButton = document.getElementById('cancel-edit');
        const refreshEventsButton = document.getElementById('refresh-events');
        const refreshMetricsButton = document.getElementById('refresh-metrics');
        const refreshRulesButton = document.getElementById('refresh-rules');
        const lastUpdateDiv = document.getElementById('last-update');
        const websocketStatusDiv = document.getElementById('websocket-status');
        const notificationDiv = document.getElementById('notification');
        const notificationOverlay = document.getElementById('notification-overlay');
        const notificationContent = document.getElementById('notification-content');
        
        // WebSocket connection
        let websocket = null;
        let reconnectTimer = null;
        
        const connectWebSocket = () => {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.hostname}:9002`; // Assuming WebSocket server on port 9002
            
            try {
                websocket = new WebSocket(wsUrl);
                
                websocket.onopen = () => {
                    console.log('WebSocket connected');
                    websocketStatusDiv.textContent = 'WebSocket: Connected';
                    websocketStatusDiv.className = 'websocket-status ws-connected';
                    
                    // Register with the server
                    const registrationMessage = {
                        type: 'register',
                        clientType: 'web_client',
                        timestamp: new Date().toISOString()
                    };
                    websocket.send(JSON.stringify(registrationMessage));
                    
                    // Clear reconnect timer if connection successful
                    if (reconnectTimer) {
                        clearTimeout(reconnectTimer);
                        reconnectTimer = null;
                    }
                };
                
                websocket.onmessage = (event) => {
                    console.log('WebSocket message received:', event.data);
                    try {
                        const message = JSON.parse(event.data);
                        showNotification(message);
                    } catch (e) {
                        // If not JSON, show as plain text
                        showNotification({ content: event.data, type: 'message' });
                    }
                };
                
                websocket.onclose = () => {
                    console.log('WebSocket disconnected');
                    websocketStatusDiv.textContent = 'WebSocket: Disconnected';
                    websocketStatusDiv.className = 'websocket-status ws-disconnected';
                    websocket = null;
                    
                    // Attempt to reconnect after 5 seconds
                    reconnectTimer = setTimeout(connectWebSocket, 5000);
                };
                
                websocket.onerror = (error) => {
                    console.error('WebSocket error:', error);
                    websocketStatusDiv.textContent = 'WebSocket: Error';
                    websocketStatusDiv.className = 'websocket-status ws-disconnected';
                };
                
            } catch (error) {
                console.error('Failed to create WebSocket connection:', error);
                websocketStatusDiv.textContent = 'WebSocket: Failed';
                websocketStatusDiv.className = 'websocket-status ws-disconnected';
                
                // Attempt to reconnect after 5 seconds
                reconnectTimer = setTimeout(connectWebSocket, 5000);
            }
        };
        
        const showNotification = (message) => {
            let content = '';
            if (typeof message === 'object') {
                if (message.content) {
                    content = message.content;
                } else if (message.summary) {
                    content = message.summary;
                } else if (message.message) {
                    content = message.message;
                } else {
                    content = JSON.stringify(message, null, 2);
                }
            } else {
                content = message.toString();
            }
            
            notificationContent.textContent = content;
            notificationOverlay.style.display = 'block';
            notificationDiv.style.display = 'block';
        };
        
        window.closeNotification = () => {
            notificationOverlay.style.display = 'none';
            notificationDiv.style.display = 'none';
        };
        
        // Close notification when clicking overlay
        notificationOverlay.addEventListener('click', closeNotification);
        
        // Initialize WebSocket connection
        connectWebSocket();
        
        // Events Pagination elements
        const eventsPaginationDiv = document.getElementById('events-pagination');
        const prevPageButton = document.getElementById('prev-page');
        const nextPageButton = document.getElementById('next-page');
        const pageInfoSpan = document.getElementById('page-info');
        const pageSizeSelect = document.getElementById('page-size-select');
        
        // Rules Pagination elements
        const rulesPaginationDiv = document.getElementById('rules-pagination');
        const rulesPrevPageButton = document.getElementById('rules-prev-page');
        const rulesNextPageButton = document.getElementById('rules-next-page');
        const rulesPageInfoSpan = document.getElementById('rules-page-info');
        const rulesPageSizeSelect = document.getElementById('rules-page-size-select');
        
        // Metrics Pagination elements
        const metricsPaginationDiv = document.getElementById('metrics-pagination');
        const metricsPrevPageButton = document.getElementById('metrics-prev-page');
        const metricsNextPageButton = document.getElementById('metrics-next-page');
        const metricsPageInfoSpan = document.getElementById('metrics-page-info');
        const metricsPageSizeSelect = document.getElementById('metrics-page-size-select');
        
        // Events Pagination state
        let currentPage = 1;
        let currentPageSize = 20;
        
        // Rules Pagination state
        let rulesCurrentPage = 1;
        let rulesCurrentPageSize = 20;
        
        // Metrics Pagination state
        let metricsCurrentPage = 1;
        let metricsCurrentPageSize = 20;

        const API_BASE = '';
        
        const updateLastRefreshTime = () => {
            const now = new Date();
            lastUpdateDiv.textContent = `Last update: ${now.toLocaleTimeString()}`;
        };

        const fetchAndRenderMetrics = async (page = metricsCurrentPage, pageSize = metricsCurrentPageSize) => {
            // Show refreshing state
            refreshMetricsButton.classList.add('refreshing');
            refreshMetricsButton.textContent = 'Refreshing...';
            refreshMetricsButton.disabled = true;
            
            try {
                const response = await fetch(`${API_BASE}/node/metrics?page=${page}&page_size=${pageSize}`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const data = await response.json();
                metricsTableBody.innerHTML = '';
                
                // Check if pagination headers are present
                const totalCount = response.headers.get('X-Total-Count');
                if (totalCount !== null) {
                    // Paginated response - read pagination info from headers
                    const pageHeader = response.headers.get('X-Page');
                    const pageSizeHeader = response.headers.get('X-Page-Size');
                    const totalPages = response.headers.get('X-Total-Pages');
                    const hasNext = response.headers.get('X-Has-Next') === 'true';
                    const hasPrev = response.headers.get('X-Has-Prev') === 'true';
                    
                    // Update pagination state
                    metricsCurrentPage = pageHeader ? parseInt(pageHeader) : page;
                    metricsCurrentPageSize = pageSizeHeader ? parseInt(pageSizeHeader) : pageSize;
                    
                    // Show pagination controls
                    metricsPaginationDiv.style.display = 'block';
                    
                    // Update pagination info
                    metricsPageInfoSpan.textContent = `Page ${metricsCurrentPage} of ${totalPages} (${totalCount} total)`;
                    
                    // Update button states
                    metricsPrevPageButton.disabled = !hasPrev;
                    metricsNextPageButton.disabled = !hasNext;
                    
                    // Update page size select
                    metricsPageSizeSelect.value = metricsCurrentPageSize;
                    
                    // For paginated response, extract data from the standardized format
                    const nodes_metrics = data.data ? data.data.nodes_metrics : data;
                    if (!Array.isArray(nodes_metrics) || nodes_metrics.length === 0) {
                        metricsTableBody.innerHTML = '<tr><td colspan="9">No node metrics found.</td></tr>';
                        return;
                    }
                    
                    nodes_metrics.forEach(node => {
                    // 修正数据结构：使用 latest_xxx_metrics
                    const cpu = node.latest_cpu_metrics || {};
                    const memory = node.latest_memory_metrics || {};
                    const diskMetrics = node.latest_disk_metrics || {};
                    const disks = diskMetrics.disks || [];
                    const networkMetrics = node.latest_network_metrics || {};
                    const networks = networkMetrics.networks || [];
                    const gpuMetrics = node.latest_gpu_metrics || {};
                    const gpus = gpuMetrics.gpus || [];
                    
                    // 计算平均磁盘使用率
                    const avgDiskUsage = disks.length > 0 ? 
                        (disks.reduce((sum, disk) => sum + (disk.usage_percent || 0), 0) / disks.length).toFixed(1) : 'N/A';
                    
                    // 计算网络总流量 (MB)
                    const totalRxMB = networks.reduce((sum, net) => sum + (net.rx_bytes || 0), 0) / (1024 * 1024);
                    const totalTxMB = networks.reduce((sum, net) => sum + (net.tx_bytes || 0), 0) / (1024 * 1024);
                    const networkInfo = networks.length > 0 ? 
                        `${totalRxMB.toFixed(1)}/${totalTxMB.toFixed(1)} MB` : 'N/A';
                    
                    // 格式化内存信息 (GB)
                    const memoryUsedGB = memory.used ? (memory.used / (1024 * 1024 * 1024)).toFixed(1) : 'N/A';
                    const memoryTotalGB = memory.total ? (memory.total / (1024 * 1024 * 1024)).toFixed(1) : 'N/A';
                    const memoryInfo = memory.used && memory.total ? 
                        `${memoryUsedGB}/${memoryTotalGB} GB` : 'N/A';
                    
                    // 格式化最后更新时间
                    const lastUpdate = cpu.timestamp ? 
                        new Date(cpu.timestamp * 1000).toLocaleString() : 'N/A';
                    
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${escapeHtml(node.host_ip)}</td>
                        <td>${(cpu.usage_percent || 0).toFixed(1)}%</td>
                        <td>${(cpu.load_avg_1m || 0).toFixed(2)}</td>
                        <td>${(memory.usage_percent || 0).toFixed(1)}%</td>
                        <td>${memoryInfo}</td>
                        <td>${avgDiskUsage}%</td>
                        <td>${networkInfo}</td>
                        <td>${gpus.length}</td>
                        <td>${lastUpdate}</td>
                    `;
                        metricsTableBody.appendChild(row);
                    });
                } else {
                    // Legacy response - hide pagination controls and use old data format
                    metricsPaginationDiv.style.display = 'none';
                    
                    // 修正字段名：使用 nodes_metrics 而不是 nodes
                    if (!data.data || !data.data.nodes_metrics || data.data.nodes_metrics.length === 0) {
                        metricsTableBody.innerHTML = '<tr><td colspan="9">No node metrics found.</td></tr>';
                        return;
                    }
                    
                    data.data.nodes_metrics.forEach(node => {
                        // 修正数据结构：使用 latest_xxx_metrics
                        const cpu = node.latest_cpu_metrics || {};
                        const memory = node.latest_memory_metrics || {};
                        const diskMetrics = node.latest_disk_metrics || {};
                        const disks = diskMetrics.disks || [];
                        const networkMetrics = node.latest_network_metrics || {};
                        const networks = networkMetrics.networks || [];
                        const gpuMetrics = node.latest_gpu_metrics || {};
                        const gpus = gpuMetrics.gpus || [];
                        
                        // 计算平均磁盘使用率
                        const avgDiskUsage = disks.length > 0 ? 
                            (disks.reduce((sum, disk) => sum + (disk.usage_percent || 0), 0) / disks.length).toFixed(1) : 'N/A';
                        
                        // 计算网络总流量 (MB)
                        const totalRxMB = networks.reduce((sum, net) => sum + (net.rx_bytes || 0), 0) / (1024 * 1024);
                        const totalTxMB = networks.reduce((sum, net) => sum + (net.tx_bytes || 0), 0) / (1024 * 1024);
                        const networkInfo = networks.length > 0 ? 
                            `${totalRxMB.toFixed(1)}/${totalTxMB.toFixed(1)} MB` : 'N/A';
                        
                        // 格式化内存信息 (GB)
                        const memoryUsedGB = memory.used ? (memory.used / (1024 * 1024 * 1024)).toFixed(1) : 'N/A';
                        const memoryTotalGB = memory.total ? (memory.total / (1024 * 1024 * 1024)).toFixed(1) : 'N/A';
                        const memoryInfo = memory.used && memory.total ? 
                            `${memoryUsedGB}/${memoryTotalGB} GB` : 'N/A';
                        
                        // 格式化最后更新时间
                        const lastUpdate = cpu.timestamp ? 
                            new Date(cpu.timestamp * 1000).toLocaleString() : 'N/A';
                        
                        const row = document.createElement('tr');
                        row.innerHTML = `
                            <td>${escapeHtml(node.host_ip)}</td>
                            <td>${(cpu.usage_percent || 0).toFixed(1)}%</td>
                            <td>${(cpu.load_avg_1m || 0).toFixed(2)}</td>
                            <td>${(memory.usage_percent || 0).toFixed(1)}%</td>
                            <td>${memoryInfo}</td>
                            <td>${avgDiskUsage}%</td>
                            <td>${networkInfo}</td>
                            <td>${gpus.length}</td>
                            <td>${lastUpdate}</td>
                        `;
                        metricsTableBody.appendChild(row);
                    });
                }
            } catch (error) {
                console.error('Error fetching node metrics:', error);
                metricsTableBody.innerHTML = '<tr><td colspan="9">Failed to load node metrics.</td></tr>';
                metricsPaginationDiv.style.display = 'none';
            } finally {
                // Reset button state
                refreshMetricsButton.classList.remove('refreshing');
                refreshMetricsButton.textContent = 'Refresh Metrics';
                refreshMetricsButton.disabled = false;
                // Update last refresh time
                updateLastRefreshTime();
            }
        };

        const fetchAndRenderRules = async (page = rulesCurrentPage, pageSize = rulesCurrentPageSize) => {
            // Show refreshing state
            refreshRulesButton.classList.add('refreshing');
            refreshRulesButton.textContent = 'Refreshing...';
            refreshRulesButton.disabled = true;
            
            try {
                const response = await fetch(`${API_BASE}/alarm/rules?page=${page}&page_size=${pageSize}`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const responseData = await response.json();
                
                // Extract rules data from the new response format
                const rules = Array.isArray(responseData) ? responseData : (responseData.data || responseData);
                
                rulesTableBody.innerHTML = '';
                
                // Check if pagination headers are present
                const totalCount = response.headers.get('X-Total-Count');
                if (totalCount !== null) {
                    // Paginated response - read pagination info from headers
                    const pageHeader = response.headers.get('X-Page');
                    const pageSizeHeader = response.headers.get('X-Page-Size');
                    const totalPages = response.headers.get('X-Total-Pages');
                    const hasNext = response.headers.get('X-Has-Next') === 'true';
                    const hasPrev = response.headers.get('X-Has-Prev') === 'true';
                    
                    // Update pagination state
                    rulesCurrentPage = pageHeader ? parseInt(pageHeader) : page;
                    rulesCurrentPageSize = pageSizeHeader ? parseInt(pageSizeHeader) : pageSize;
                    
                    // Show pagination controls
                    rulesPaginationDiv.style.display = 'block';
                    
                    // Update pagination info
                    rulesPageInfoSpan.textContent = `Page ${rulesCurrentPage} of ${totalPages} (${totalCount} total)`;
                    
                    // Update button states
                    rulesPrevPageButton.disabled = !hasPrev;
                    rulesNextPageButton.disabled = !hasNext;
                    
                    // Update page size select
                    rulesPageSizeSelect.value = rulesCurrentPageSize;
                } else {
                    // Legacy response - hide pagination controls
                    rulesPaginationDiv.style.display = 'none';
                }
                
                if (!Array.isArray(rules) || rules.length === 0) {
                    rulesTableBody.innerHTML = '<tr><td colspan="6">No alarm rules found.</td></tr>';
                    return;
                }
                
                rules.forEach(rule => {
                    // Format expression for display (it should already be a parsed object from the backend)
                    let expressionDisplay = '';
                    try {
                        expressionDisplay = JSON.stringify(rule.expression, null, 2);
                    } catch (e) {
                        expressionDisplay = rule.expression || 'Invalid JSON';
                    }
                    
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${escapeHtml(rule.alert_name)}</td>
                        <td><pre>${escapeHtml(expressionDisplay)}</pre></td>
                        <td>${escapeHtml(rule.for || rule.for_duration)}</td>
                        <td>${escapeHtml(rule.severity)}</td>
                        <td>${escapeHtml(rule.summary)}</td>
                        <td class="actions-cell">
                            <button class="edit-btn" onclick="editRule('${rule.id}')">Edit</button>
                            <button class="delete-btn" onclick="deleteRule('${rule.id}')">Delete</button>
                        </td>
                    `;
                    rulesTableBody.appendChild(row);
                });
            } catch (error) {
                console.error('Error fetching rules:', error);
                rulesTableBody.innerHTML = '<tr><td colspan="6">Failed to load alarm rules.</td></tr>';
                rulesPaginationDiv.style.display = 'none';
            } finally {
                // Reset button state
                refreshRulesButton.classList.remove('refreshing');
                refreshRulesButton.textContent = 'Refresh Rules';
                refreshRulesButton.disabled = false;
                // Update last refresh time only if this isn't an automatic refresh
                updateLastRefreshTime();
            }
        };

        const fetchAndRenderEvents = async (page = currentPage, pageSize = currentPageSize) => {
            // Show refreshing state
            refreshEventsButton.classList.add('refreshing');
            refreshEventsButton.textContent = 'Refreshing...';
            refreshEventsButton.disabled = true;
            
            try {
                const response = await fetch(`${API_BASE}/alarm/events?page=${page}&page_size=${pageSize}`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const responseData = await response.json();
                
                // Extract events data from the new response format
                const events = Array.isArray(responseData) ? responseData : (responseData.data || responseData);
                
                eventsTableBody.innerHTML = '';
                
                // Check if pagination headers are present
                const totalCount = response.headers.get('X-Total-Count');
                if (totalCount !== null) {
                    // Paginated response - read pagination info from headers
                    const pageHeader = response.headers.get('X-Page');
                    const pageSizeHeader = response.headers.get('X-Page-Size');
                    const totalPages = response.headers.get('X-Total-Pages');
                    const hasNext = response.headers.get('X-Has-Next') === 'true';
                    const hasPrev = response.headers.get('X-Has-Prev') === 'true';
                    
                    // Update pagination state
                    currentPage = pageHeader ? parseInt(pageHeader) : page;
                    currentPageSize = pageSizeHeader ? parseInt(pageSizeHeader) : pageSize;
                    
                    // Show pagination controls
                    eventsPaginationDiv.style.display = 'block';
                    
                    // Update pagination info
                    pageInfoSpan.textContent = `Page ${currentPage} of ${totalPages} (${totalCount} total)`;
                    
                    // Update button states
                    prevPageButton.disabled = !hasPrev;
                    nextPageButton.disabled = !hasNext;
                    
                    // Update page size select
                    pageSizeSelect.value = currentPageSize;
                } else {
                    // Legacy response - hide pagination controls
                    eventsPaginationDiv.style.display = 'none';
                }
                
                if (!Array.isArray(events) || events.length === 0) {
                    eventsTableBody.innerHTML = '<tr><td colspan="5">No alarm events found.</td></tr>';
                    return;
                }
                
                events.forEach(event => {
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${escapeHtml(event.status)}</td>
                        <td><pre>${escapeHtml(JSON.stringify(event.labels, null, 2))}</pre></td>
                        <td><pre>${escapeHtml(JSON.stringify(event.annotations, null, 2))}</pre></td>
                        <td>${new Date(event.starts_at).toLocaleString()}</td>
                        <td>${event.ends_at ? new Date(event.ends_at).toLocaleString() : 'N/A'}</td>
                    `;
                    eventsTableBody.appendChild(row);
                });
            } catch (error) {
                console.error('Error fetching events:', error);
                eventsTableBody.innerHTML = '<tr><td colspan="5">Failed to load alarm events.</td></tr>';
                eventsPaginationDiv.style.display = 'none';
            } finally {
                // Reset button state
                refreshEventsButton.classList.remove('refreshing');
                refreshEventsButton.textContent = 'Refresh Events';
                refreshEventsButton.disabled = false;
                // Update last refresh time
                updateLastRefreshTime();
            }
        };

        const resetForm = () => {
            ruleForm.reset();
            ruleIdInput.value = '';
            document.querySelector('h3').innerText = 'Add / Edit Rule';
        };

        ruleForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const id = ruleIdInput.value;
            const url = id ? `${API_BASE}/alarm/rules/${id}/update` : `${API_BASE}/alarm/rules`;
            const method = 'POST';

            let expression;
            try {
                expression = JSON.parse(document.getElementById('expression').value);
            } catch (err) {
                alert('Expression is not valid JSON.');
                return;
            }

            const body = {
                alert_name: document.getElementById('alert_name').value,
                expression: expression,
                for: document.getElementById('for_duration').value,
                severity: document.getElementById('severity').value,
                summary: document.getElementById('summary').value,
                description: document.getElementById('description').value,
                alert_type: document.getElementById('alert_type').value,
            };

            try {
                const response = await fetch(url, {
                    method: method,
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(body)
                });
                if (!response.ok) {
                    const errData = await response.json();
                    throw new Error(errData.error || `HTTP error! status: ${response.status}`);
                }
                const result = await response.json();
                // Check if response is in standard format and verify success
                if (result.status && result.status !== 'success') {
                    throw new Error('Operation failed');
                }
                resetForm();
                fetchAndRenderRules();
            } catch (error) {
                console.error('Error saving rule:', error);
                alert(`Failed to save alarm rule: ${error.message}`);
            }
        });

        window.editRule = async (id) => {
            try {
                const response = await fetch(`${API_BASE}/alarm/rules/${id}`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const responseData = await response.json();
                
                // Extract rule data from the new response format
                const rule = responseData.data || responseData; // Support both new and legacy formats
                
                document.querySelector('h3').innerText = `Edit Rule: ${escapeHtml(rule.alert_name)}`;
                ruleIdInput.value = rule.id;
                document.getElementById('alert_name').value = rule.alert_name;
                
                // Format expression for editing (it should already be a parsed object from the backend)
                let expressionValue = '';
                try {
                    expressionValue = JSON.stringify(rule.expression, null, 2);
                } catch (e) {
                    expressionValue = rule.expression || '{}';
                }
                document.getElementById('expression').value = expressionValue;
                
                document.getElementById('for_duration').value = rule.for || rule.for_duration;
                document.getElementById('severity').value = rule.severity;
                document.getElementById('summary').value = rule.summary;
                document.getElementById('description').value = rule.description;
                document.getElementById('alert_type').value = rule.alert_type;
                
                ruleForm.scrollIntoView({ behavior: 'smooth' });
                document.getElementById('alert_name').focus();
            } catch (error) {
                console.error('Error fetching rule for edit:', error);
                alert('Failed to fetch rule details.');
            }
        };

        window.deleteRule = async (id) => {
            if (!confirm('Are you sure you want to delete this rule?')) return;
            try {
                const response = await fetch(`${API_BASE}/alarm/rules/${id}/delete`, { method: 'POST' });
                if (!response.ok) {
                    const errData = await response.json();
                    throw new Error(errData.error || `HTTP error! status: ${response.status}`);
                }
                const result = await response.json();
                // Check if response is in standard format and verify success
                if (result.status && result.status !== 'success') {
                    throw new Error('Delete operation failed');
                }
                fetchAndRenderRules();
            } catch (error) {
                console.error('Error deleting rule:', error);
                alert(`Failed to delete alarm rule: ${error.message}`);
            }
        };

        function escapeHtml(unsafe) {
            if (unsafe === null || unsafe === undefined) return '';
            return unsafe
                .toString()
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#039;");
        }

        cancelEditButton.addEventListener('click', resetForm);
        refreshEventsButton.addEventListener('click', () => fetchAndRenderEvents(currentPage, currentPageSize));
        refreshMetricsButton.addEventListener('click', () => fetchAndRenderMetrics(metricsCurrentPage, metricsCurrentPageSize));
        refreshRulesButton.addEventListener('click', () => fetchAndRenderRules(rulesCurrentPage, rulesCurrentPageSize));
        
        // Events Pagination event listeners
        prevPageButton.addEventListener('click', () => {
            if (currentPage > 1) {
                fetchAndRenderEvents(currentPage - 1, currentPageSize);
            }
        });
        
        nextPageButton.addEventListener('click', () => {
            fetchAndRenderEvents(currentPage + 1, currentPageSize);
        });
        
        pageSizeSelect.addEventListener('change', (e) => {
            const newPageSize = parseInt(e.target.value);
            currentPageSize = newPageSize;
            // Reset to page 1 when changing page size
            fetchAndRenderEvents(1, newPageSize);
        });
        
        // Rules Pagination event listeners
        rulesPrevPageButton.addEventListener('click', () => {
            if (rulesCurrentPage > 1) {
                fetchAndRenderRules(rulesCurrentPage - 1, rulesCurrentPageSize);
            }
        });
        
        rulesNextPageButton.addEventListener('click', () => {
            fetchAndRenderRules(rulesCurrentPage + 1, rulesCurrentPageSize);
        });
        
        rulesPageSizeSelect.addEventListener('change', (e) => {
            const newPageSize = parseInt(e.target.value);
            rulesCurrentPageSize = newPageSize;
            // Reset to page 1 when changing page size
            fetchAndRenderRules(1, newPageSize);
        });
        
        // Metrics Pagination event listeners
        metricsPrevPageButton.addEventListener('click', () => {
            if (metricsCurrentPage > 1) {
                fetchAndRenderMetrics(metricsCurrentPage - 1, metricsCurrentPageSize);
            }
        });
        
        metricsNextPageButton.addEventListener('click', () => {
            fetchAndRenderMetrics(metricsCurrentPage + 1, metricsCurrentPageSize);
        });
        
        metricsPageSizeSelect.addEventListener('change', (e) => {
            const newPageSize = parseInt(e.target.value);
            metricsCurrentPageSize = newPageSize;
            // Reset to page 1 when changing page size
            fetchAndRenderMetrics(1, newPageSize);
        });

        fetchAndRenderMetrics();
        fetchAndRenderRules();
        fetchAndRenderEvents();
        
        // Auto-refresh all data periodically
        setInterval(fetchAndRenderMetrics, 10000);    // Node metrics every 10 seconds (most dynamic)
        setInterval(fetchAndRenderEvents, 20000);     // Alarm events every 20 seconds (dynamic)
        setInterval(fetchAndRenderRules, 60000);      // Alarm rules every 60 seconds (less dynamic)
    });
    </script>
</body>
</html>
)HTML";
}

HttpServer::HttpServer(std::shared_ptr<ResourceStorage> resource_storage,
                       std::shared_ptr<AlarmRuleStorage> alarm_rule_storage,
                       std::shared_ptr<AlarmManager> alarm_manager,
                       std::shared_ptr<NodeStorage> node_storage,
                       std::shared_ptr<ResourceManager> resource_manager,
                       std::shared_ptr<BMCStorage> bmc_storage,
                       std::shared_ptr<ChassisController> chassis_controller,
                       const std::string &host, int port)
    : m_resource_storage(resource_storage), m_alarm_rule_storage(alarm_rule_storage),
      m_alarm_manager(alarm_manager), m_node_storage(node_storage),
      m_resource_manager(resource_manager), m_bmc_storage(bmc_storage),
      m_chassis_controller(chassis_controller), m_host(host), m_port(port)
{
    setup_routes();
}

HttpServer::~HttpServer()
{
    stop();
}

bool HttpServer::start()
{
    if (m_server.is_running())
    {
        return true;
    }

    m_server.set_default_headers({{"Access-Control-Allow-Origin", "*"},
                                  {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
                                  {"Access-Control-Allow-Headers", "Content-Type"}});

    m_server.Options(".*", [](const httplib::Request &req, httplib::Response &res)
                     { res.set_content("OK", "text/plain"); });

    m_server_thread = std::thread([this]()
                                  {
        LogManager::getLogger()->info("HTTP server starting on {}:{}", m_host, m_port);
        if (!m_server.listen(m_host.c_str(), m_port)) {
            LogManager::getLogger()->error("HTTP server failed to start.");
        } });

    // Give the server a moment to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return m_server.is_running();
}

void HttpServer::stop()
{
    if (m_server.is_running())
    {
        m_server.stop();
        if (m_server_thread.joinable())
        {
            m_server_thread.join();
        }
        LogManager::getLogger()->info("HTTP server stopped.");
    }
}

void HttpServer::setup_routes()
{
    m_server.Get("/", [this](const httplib::Request &, httplib::Response &res)
                 { res.set_content(get_web_page_html(), "text/html"); });

    // 节点心跳路由
    m_server.Post("/heartbeat", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_heart(req, res); });

    // 节点资源数据路由
    m_server.Post("/resource", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_resource(req, res); });

    // 节点数据查询路由
    m_server.Get("/node", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_nodes_list(req, res); });

    // 节点指标查询路由
    m_server.Get("/node/metrics", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_node_metrics(req, res); });

    // 节点历史指标查询路由
    m_server.Get("/node/historical-metrics", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_node_historical_metrics(req, res); });

    m_server.Get("/node/historical-bmc", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_node_historical_bmc(req, res); });

    // 告警规则相关路由
    m_server.Post("/alarm/rules", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_alarm_rules_create(req, res); });

    m_server.Get("/alarm/rules", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_alarm_rules_list(req, res); });

    m_server.Get(R"(/alarm/rules/([^/]+))", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_alarm_rules_get(req, res); });

    m_server.Post(R"(/alarm/rules/([^/]+)/update)", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_alarm_rules_update(req, res); });

    m_server.Post(R"(/alarm/rules/([^/]+)/delete)", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_alarm_rules_delete(req, res); });

    // 告警事件相关路由
    m_server.Get("/alarm/events", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_alarm_events_list(req, res); });

    m_server.Get("/alarm/events/count", [this](const httplib::Request &req, httplib::Response &res)
                 { this->handle_alarm_events_count(req, res); });

    // 机箱控制相关路由
    m_server.Post("/chassis/reset", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_chassis_reset(req, res); });

    m_server.Post("/chassis/power-off", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_chassis_power_off(req, res); });

    m_server.Post("/chassis/power-on", [this](const httplib::Request &req, httplib::Response &res)
                  { this->handle_chassis_power_on(req, res); });
}

void HttpServer::handle_alarm_rules_create(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        json body = json::parse(req.body);

        std::string id = m_alarm_rule_storage->insertAlarmRule(
            body["alert_name"].get<std::string>(),
            body["expression"].get<nlohmann::json>(),
            body["for"].get<std::string>(),
            body["severity"].get<std::string>(),
            body["summary"].get<std::string>(),
            body["description"].get<std::string>(),
            body["alert_type"].get<std::string>(),
            true);
        if (id.empty())
        {
            res.set_content("{\"error\":\"Failed to store alarm rules\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to store alarm rules");
            return;
        }

        json response = {
            {"api_version", 1},
            {"status", "success"},
            {"data", {{"id", id}}}};

        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully processed alarm rules");
    }
    catch (const json::parse_error &e)
    {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_create: {}", e.what());
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"An unexpected error occurred\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_create: {}", e.what());
    }
}

void HttpServer::handle_alarm_rules_list(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        // 获取查询参数
        std::string page_str = req.get_param_value("page");
        std::string page_size_str = req.get_param_value("page_size");
        std::string enabled_only_str = req.get_param_value("enabled_only");

        // 检查是否使用分页模式
        bool use_pagination = !page_str.empty() || !page_size_str.empty();

        if (use_pagination)
        {
            // 分页模式
            int page = page_str.empty() ? 1 : std::stoi(page_str);
            int page_size = page_size_str.empty() ? 20 : std::stoi(page_size_str);
            bool enabled_only = enabled_only_str == "true";

            // 验证参数范围
            if (page < 1)
                page = 1;
            if (page_size < 1)
                page_size = 20;
            if (page_size > 1000)
                page_size = 1000;

            // 使用分页API
            auto paginated_result = m_alarm_rule_storage->getPaginatedAlarmRules(page, page_size, enabled_only);

            json result_data = json::array();

            // 直接添加规则数据到数组
            for (const auto &rule : paginated_result.rules)
            {
                json rule_json = {
                    {"id", rule.id},
                    {"alert_name", rule.alert_name},
                    {"expression", json::parse(rule.expression_json)},
                    {"for", rule.for_duration},
                    {"severity", rule.severity},
                    {"summary", rule.summary},
                    {"description", rule.description},
                    {"alert_type", rule.alert_type},
                    {"enabled", rule.enabled},
                    {"created_at", rule.created_at},
                    {"updated_at", rule.updated_at}};
                result_data.push_back(rule_json);
            }

            // 通过HTTP头部传递分页信息
            res.set_header("X-Page", std::to_string(paginated_result.page));
            res.set_header("X-Page-Size", std::to_string(paginated_result.page_size));
            res.set_header("X-Total-Count", std::to_string(paginated_result.total_count));
            res.set_header("X-Total-Pages", std::to_string(paginated_result.total_pages));
            res.set_header("X-Has-Next", paginated_result.has_next ? "true" : "false");
            res.set_header("X-Has-Prev", paginated_result.has_prev ? "true" : "false");

            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", result_data}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully retrieved {} alarm rules (page {}/{}, total: {})",
                                           paginated_result.rules.size(), paginated_result.page,
                                           paginated_result.total_pages, paginated_result.total_count);
        }
        else
        {
            // 兼容模式 - 使用旧的API
            auto rules = m_alarm_rule_storage->getAllAlarmRules();

            json result_data = json::array();
            for (const auto &rule : rules)
            {
                json rule_json = {
                    {"id", rule.id},
                    {"alert_name", rule.alert_name},
                    {"expression", json::parse(rule.expression_json)},
                    {"for", rule.for_duration},
                    {"severity", rule.severity},
                    {"summary", rule.summary},
                    {"description", rule.description},
                    {"alert_type", rule.alert_type},
                    {"enabled", rule.enabled},
                    {"created_at", rule.created_at},
                    {"updated_at", rule.updated_at}};
                result_data.push_back(rule_json);
            }

            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", result_data}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully retrieved {} alarm rules (legacy mode)", rules.size());
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve alarm rules\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_list: {}", e.what());
    }
}

void HttpServer::handle_alarm_rules_get(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string rule_id = req.matches[1];

        AlarmRule rule = m_alarm_rule_storage->getAlarmRule(rule_id);

        if (rule.id.empty())
        {
            res.set_content("{\"error\":\"Alarm rule not found\"}", "application/json");
            res.status = 404;
            LogManager::getLogger()->warn("Alarm rule not found: {}", rule_id);
            return;
        }

        json rule_data = {
            {"id", rule.id},
            {"alert_name", rule.alert_name},
            {"expression", json::parse(rule.expression_json)},
            {"for", rule.for_duration},
            {"severity", rule.severity},
            {"summary", rule.summary},
            {"description", rule.description},
            {"alert_type", rule.alert_type},
            {"enabled", rule.enabled},
            {"created_at", rule.created_at},
            {"updated_at", rule.updated_at}};

        json response = {
            {"api_version", 1},
            {"status", "success"},
            {"data", rule_data}};

        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->debug("Successfully retrieved alarm rule: {}", rule_id);
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve alarm rule\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_get: {}", e.what());
    }
}

void HttpServer::handle_alarm_rules_update(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string rule_id = req.matches[1];
        json body = json::parse(req.body);

        // 检查规则是否存在
        AlarmRule existing_rule = m_alarm_rule_storage->getAlarmRule(rule_id);
        if (existing_rule.id.empty())
        {
            res.set_content("{\"error\":\"Alarm rule not found\"}", "application/json");
            res.status = 404;
            LogManager::getLogger()->warn("Alarm rule not found for update: {}", rule_id);
            return;
        }

        // 更新规则
        bool success = m_alarm_rule_storage->updateAlarmRule(
            rule_id,
            body["alert_name"].get<std::string>(),
            body["expression"].get<nlohmann::json>(),
            body["for"].get<std::string>(),
            body["severity"].get<std::string>(),
            body["summary"].get<std::string>(),
            body["description"].get<std::string>(),
            body["alert_type"].get<std::string>(),
            body.value("enabled", true));

        if (success)
        {
            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", {{"id", rule_id}}}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->info("Successfully updated alarm rule: {}", rule_id);
        }
        else
        {
            res.set_content("{\"error\":\"Failed to update alarm rule\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to update alarm rule: {}", rule_id);
        }
    }
    catch (const json::parse_error &e)
    {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_alarm_rules_update: {}", e.what());
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to update alarm rule\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_update: {}", e.what());
    }
}

void HttpServer::handle_alarm_rules_delete(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string rule_id = req.matches[1];

        // 检查规则是否存在
        AlarmRule existing_rule = m_alarm_rule_storage->getAlarmRule(rule_id);
        if (existing_rule.id.empty())
        {
            res.set_content("{\"error\":\"Alarm rule not found\"}", "application/json");
            res.status = 404;
            LogManager::getLogger()->warn("Alarm rule not found for deletion: {}", rule_id);
            return;
        }

        // 删除规则
        bool success = m_alarm_rule_storage->deleteAlarmRule(rule_id);

        if (success)
        {
            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", {{"id", rule_id}}}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->info("Successfully deleted alarm rule: {}", rule_id);
        }
        else
        {
            res.set_content("{\"error\":\"Failed to delete alarm rule\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to delete alarm rule: {}", rule_id);
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to delete alarm rule\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_delete: {}", e.what());
    }
}

void HttpServer::handle_resource(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        json body = json::parse(req.body);

        if (!body.contains("data") || !body["data"].is_object())
        {
            res.set_content("{\"error\":\"'data' field is missing or not an object\"}", "application/json");
            res.status = 400;
            return;
        }
        const auto &data = body["data"];

        if (!data.contains("host_ip") || !data["host_ip"].is_string())
        {
            res.set_content("{\"error\":\"'host_ip' is missing or not a string\"}", "application/json");
            res.status = 400;
            return;
        }

        if (!data.contains("resource") || !data["resource"].is_object())
        {
            res.set_content("{\"error\":\"'resource' field is missing or not an object\"}", "application/json");
            res.status = 400;
            return;
        }

        // 将JSON数据反序列化为ResourceInfo结构体
        node::ResourceInfo resource_info = data.get<node::ResourceInfo>();

        if (m_resource_storage->insertResourceData(resource_info.host_ip, resource_info))
        {
            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", {}}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully processed resource data for host: {}", resource_info.host_ip);
        }
        else
        {
            res.set_content("{\"error\":\"Failed to store resource data\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to store resource data for host: {}", resource_info.host_ip);
        }
    }
    catch (const json::parse_error &e)
    {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_resource: {}", e.what());
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"An unexpected error occurred\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_resource: {}", e.what());
    }
}

void HttpServer::handle_alarm_events_list(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!m_alarm_manager)
        {
            res.set_content("{\"error\":\"Alarm manager not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Alarm manager not available");
            return;
        }

        // 获取查询参数
        std::string status = req.get_param_value("status");
        std::string page_str = req.get_param_value("page");
        std::string page_size_str = req.get_param_value("page_size");
        std::string limit_str = req.get_param_value("limit"); // 兼容旧参数

        // 检查是否使用分页模式
        bool use_pagination = !page_str.empty() || !page_size_str.empty();

        if (use_pagination)
        {
            // 分页模式
            int page = page_str.empty() ? 1 : std::stoi(page_str);
            int page_size = page_size_str.empty() ? 20 : std::stoi(page_size_str);

            // 验证参数范围
            if (page < 1)
                page = 1;
            if (page_size < 1)
                page_size = 20;
            if (page_size > 1000)
                page_size = 1000;

            // 使用分页API
            auto paginated_result = m_alarm_manager->getPaginatedAlarmEvents(page, page_size, status);

            json result_data = json::array();

            // 直接添加事件数据到数组
            for (const auto &event : paginated_result.events)
            {
                json event_json = {
                    {"id", event.id},
                    {"fingerprint", event.fingerprint},
                    {"status", event.status},
                    {"labels", json::parse(event.labels_json)},
                    {"annotations", json::parse(event.annotations_json)},
                    {"starts_at", event.starts_at},
                    {"ends_at", event.ends_at},
                    {"generator_url", event.generator_url},
                    {"created_at", event.created_at},
                    {"updated_at", event.updated_at}};
                result_data.push_back(event_json);
            }

            // 通过HTTP头部传递分页信息
            res.set_header("X-Page", std::to_string(paginated_result.page));
            res.set_header("X-Page-Size", std::to_string(paginated_result.page_size));
            res.set_header("X-Total-Count", std::to_string(paginated_result.total_count));
            res.set_header("X-Total-Pages", std::to_string(paginated_result.total_pages));
            res.set_header("X-Has-Next", paginated_result.has_next ? "true" : "false");
            res.set_header("X-Has-Prev", paginated_result.has_prev ? "true" : "false");

            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", result_data}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully retrieved {} alarm events (page {}/{}, total: {})",
                                           paginated_result.events.size(), paginated_result.page,
                                           paginated_result.total_pages, paginated_result.total_count);
        }
        else
        {
            // 兼容模式 - 使用旧的API
            std::vector<AlarmEventRecord> events;

            if (status == "active" || status == "firing")
            {
                // 获取活跃的告警事件
                events = m_alarm_manager->getActiveAlarmEvents();
            }
            else if (!limit_str.empty())
            {
                // 获取最近的告警事件（带限制）
                int limit = std::stoi(limit_str);
                events = m_alarm_manager->getRecentAlarmEvents(limit);
            }
            else
            {
                // 获取最近的告警事件（默认限制100条）
                events = m_alarm_manager->getRecentAlarmEvents(100);
            }

            json result_data = json::array();
            for (const auto &event : events)
            {
                json event_json = {
                    {"id", event.id},
                    {"fingerprint", event.fingerprint},
                    {"status", event.status},
                    {"labels", json::parse(event.labels_json)},
                    {"annotations", json::parse(event.annotations_json)},
                    {"starts_at", event.starts_at},
                    {"ends_at", event.ends_at},
                    {"generator_url", event.generator_url},
                    {"created_at", event.created_at},
                    {"updated_at", event.updated_at}};
                result_data.push_back(event_json);
            }

            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", result_data}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully retrieved {} alarm events (legacy mode)", events.size());
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve alarm events\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_events_list: {}", e.what());
    }
}

void HttpServer::handle_alarm_events_count(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!m_alarm_manager)
        {
            res.set_content("{\"error\":\"Alarm manager not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Alarm manager not available");
            return;
        }

        // 获取查询参数
        std::string status = req.get_param_value("status");

        int count = 0;
        if (status == "active" || status == "firing")
        {
            // 获取活跃告警数量
            count = m_alarm_manager->getActiveAlarmCount();
            LogManager::getLogger()->debug("Successfully retrieved active alarm events count: {}", count);
        }
        else
        {
            // 获取告警事件总数量
            count = m_alarm_manager->getTotalAlarmCount();
            LogManager::getLogger()->debug("Successfully retrieved total alarm events count: {}", count);
        }

        json response = {
            {"api_version", 1},
            {"status", "success"},
            {"data", {{"count", count}}}};

        res.set_content(response.dump(2), "application/json");
        res.status = 200;
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve alarm events count\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_events_count: {}", e.what());
    }
}

void HttpServer::handle_heart(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        json body = json::parse(req.body);

        if (!body.contains("data") || !body["data"].is_object())
        {
            res.set_content("{\"error\":\"'data' field is missing or not an object\"}", "application/json");
            res.status = 400;
            LogManager::getLogger()->warn("Heart request missing 'data' field");
            return;
        }

        // 尝试反序列化为 node::BoxInfo
        node::BoxInfo node_info = body["data"].get<node::BoxInfo>();

        if (!m_node_storage)
        {
            res.set_content("{\"error\":\"Node storage not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Node storage not available for heart request");
            return;
        }

        if (m_node_storage->storeBoxInfo(node_info))
        {
            json response = {
                {"api_version", 1},
                {"status", "success"},
                {"data", {}}};

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully processed heart data for node: {}", node_info.host_ip);
        }
        else
        {
            res.set_content("{\"error\":\"Failed to store node data\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to store heart data for node: {}", node_info.host_ip);
        }
    }
    catch (const json::parse_error &e)
    {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_heart: {}", e.what());
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"An unexpected error occurred\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_heart: {}", e.what());
    }
}

void HttpServer::handle_nodes_list(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string host_ip = req.get_param_value("host_ip");
        if (!host_ip.empty())
        {
            auto node = m_resource_manager->getNode(host_ip);

            if (node)
            {
                nlohmann::json node_data = *node;
                json response = {
                    {"api_version", 1},
                    {"data", node_data},
                    {"status", "success"}
                };
                
                res.set_content(response.dump(2), "application/json");
                res.status = 200;
                LogManager::getLogger()->debug("Successfully retrieved node data for host_ip: {} using ResourceManager", host_ip);
            }
            else
            {
                res.set_content("{\"error\":\"Node not found\"}", "application/json");
                res.status = 404;
                LogManager::getLogger()->error("ResourceManager failed to retrieve node data for host_ip: {}", host_ip);
            }
        }
        else
        {
            // 使用ResourceManager获取节点列表数据
            auto node_list = m_resource_manager->getNodesList();

            if (!node_list.nodes.empty())
            {
                // 直接使用NodeDataList的JSON序列化
                json nodes_json = node_list.nodes;
                
                // 构建符合要求的响应格式
                json response = {
                    {"api_version", 1},
                    {"data", {
                        {"nodes", nodes_json}
                    }},
                    {"status", "success"}
                };
                
                res.set_content(response.dump(2), "application/json");
                res.status = 200;
                LogManager::getLogger()->debug("Successfully retrieved {} nodes list using ResourceManager", node_list.nodes.size());
            }
            else
            {
                res.set_content("{\"error\":\"No nodes found\"}", "application/json");
                res.status = 404;
                LogManager::getLogger()->error("ResourceManager failed to retrieve nodes list: no nodes found");
            }
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve nodes data\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_nodes_list: {}", e.what());
    }
}

void HttpServer::handle_node_metrics(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string page_str = req.get_param_value("page");
        std::string page_size_str = req.get_param_value("page_size");

        int page = page_str.empty() ? 1 : std::stoi(page_str);
        int page_size = page_size_str.empty() ? 1000 : std::stoi(page_size_str);

        auto paginated_result = m_resource_manager->getPaginatedCurrentMetrics(page, page_size);

        if (paginated_result.success)
        {
            nlohmann::json json_data = paginated_result.data;
            nlohmann::json pagination_data = paginated_result.pagination;
            json response = {
                {"api_version", 1},
                {"data", {{"nodes_metrics", json_data["nodes_metrics"]}}},
                {"pagination", pagination_data},
                {"status", "success"}};

            // 通过HTTP头部传递分页信息
            res.set_header("X-Page", std::to_string(paginated_result.pagination.page));
            res.set_header("X-Page-Size", std::to_string(paginated_result.pagination.page_size));
            res.set_header("X-Total-Count", std::to_string(paginated_result.pagination.total_count));
            res.set_header("X-Total-Pages", std::to_string(paginated_result.pagination.total_pages));
            res.set_header("X-Has-Next", paginated_result.pagination.has_next ? "true" : "false");
            res.set_header("X-Has-Prev", paginated_result.pagination.has_prev ? "true" : "false");

            res.set_content(response.dump(2), "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully retrieved {} node metrics (page {}/{}, total: {})",
                                           paginated_result.data.nodes_metrics.size(), paginated_result.pagination.page,
                                           paginated_result.pagination.total_pages, paginated_result.pagination.total_count);
        }
        else
        {
            res.set_content("{\"error\":\"" + paginated_result.error_message + "\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("ResourceManager failed to retrieve paginated node metrics: {}", paginated_result.error_message);
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve node metrics\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_node_metrics: {}", e.what());
    }
}

void HttpServer::handle_node_historical_metrics(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!m_resource_manager)
        {
            res.set_content("{\"error\":\"Resource manager not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Resource manager not available for historical metrics request");
            return;
        }

        // 构建请求对象
        HistoricalMetricsRequest request;
        request.host_ip = req.get_param_value("host_ip");
        request.time_range = req.get_param_value("time_range");
        if (request.time_range.empty())
        {
            request.time_range = "10m"; // 默认1小时
        }

        // 解析metrics参数
        std::string metrics_param = req.get_param_value("metrics");
        request.metrics = m_resource_manager->parseMetricsParam(metrics_param);

        // 调用ResourceManager获取历史数据
        auto response_data = m_resource_manager->getHistoricalMetrics(request);

        if (response_data.success)
        {
            json json_response = response_data.data.to_json();
            json response_data_json = {
                {"api_version", 1},
                {"status", "success"},
                {"data", {
                    {"historical_metrics", json_response}
                }}
            };
            res.set_content(response_data_json.dump(2), "application/json");
            res.status = 200;
        }
        else
        {
            res.set_content("{\"error\":\"Failed to retrieve historical metrics\"}", "application/json");
            res.status = 500;
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve historical metrics\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_node_historical_metrics: {}", e.what());
    }
}

void HttpServer::handle_node_historical_bmc(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!m_resource_manager)
        {
            res.set_content("{\"error\":\"Resource manager not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Resource manager not available for historical bmc request");
            return;
        }

        // 构建请求对象
        HistoricalBMCRequest request;
        request.box_id = std::stoi(req.get_param_value("box_id"));
        request.time_range = req.get_param_value("time_range");
        if (request.time_range.empty())
        {
            request.time_range = "1h"; // 默认1小时
        }

        // 解析metrics参数
        std::string metrics_param = req.get_param_value("metrics");
        request.metrics = m_resource_manager->parseMetricsParam(metrics_param);

        // 调用ResourceManager获取历史数据
        auto response_data = m_resource_manager->getHistoricalBMC(request);

        // 格式化响应
        auto json_response = response_data.data.to_json();

        if (response_data.success)
        {
            res.set_content(json_response.dump(2), "application/json");
            res.status = 200;
        }
        else
        {
            res.set_content(json_response.dump(2), "application/json");
            res.status = 400;
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to retrieve historical bmc\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_node_historical_bmc: {}", e.what());
    }
}

void HttpServer::handle_chassis_reset(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!m_chassis_controller)
        {
            res.set_content("{\"error\":\"Chassis controller not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Chassis controller not available for reset request");
            return;
        }

        json body = json::parse(req.body);

        // 验证必需的字段
        if (!body.contains("target_ip") || !body["target_ip"].is_string())
        {
            res.set_content("{\"error\":\"'target_ip' field is required and must be a string\"}", "application/json");
            res.status = 400;
            return;
        }

        std::string target_ip = body["target_ip"].get<std::string>();
        int req_id = body.value("request_id", 0);

        ChassisController::OperationResponse result;

        if (body.contains("slots") && body["slots"].is_array())
        {
            // 多槽位操作
            std::vector<int> slot_numbers;
            for (const auto &slot : body["slots"])
            {
                if (slot.is_number_integer())
                {
                    slot_numbers.push_back(slot.get<int>());
                }
            }

            if (slot_numbers.empty())
            {
                res.set_content("{\"error\":\"No valid slot numbers provided\"}", "application/json");
                res.status = 400;
                return;
            }

            result = m_chassis_controller->resetChassisBoards(target_ip, slot_numbers, req_id);
        }
        else if (body.contains("slot") && body["slot"].is_number_integer())
        {
            // 单槽位操作
            int slot_number = body["slot"].get<int>();
            result = m_chassis_controller->resetChassisBoard(target_ip, slot_number, req_id);
        }
        else
        {
            res.set_content("{\"error\":\"Either 'slot' (integer) or 'slots' (array) field is required\"}", "application/json");
            res.status = 400;
            return;
        }

        // 构建响应
        json slot_results = json::array();
        for (const auto &slot_result : result.slot_results)
        {
            json slot_json = {
                {"slot_number", slot_result.slot_number},
                {"status", static_cast<int>(slot_result.status)},
                {"status_text", [&]()
                 {
                     switch (slot_result.status)
                     {
                     case ChassisController::SlotStatus::SUCCESS:
                         return "success";
                     case ChassisController::SlotStatus::FAILED:
                         return "failed";
                     case ChassisController::SlotStatus::REQUEST_OPERATION:
                         return "requested";
                     default:
                         return "no_operation";
                     }
                 }()}};
            slot_results.push_back(slot_json);
        }

        json response = {
            {"api_version", 1},
            {"status", "success"},
            {"data", {{"operation", "reset"}, {"target_ip", target_ip}, {"request_id", req_id}, {"result", static_cast<int>(result.result)}, {"result_text", [&]()
                                                                                                                                              {
                                                                                                                                                  switch (result.result)
                                                                                                                                                  {
                                                                                                                                                  case ChassisController::OperationResult::SUCCESS:
                                                                                                                                                      return "success";
                                                                                                                                                  case ChassisController::OperationResult::PARTIAL_SUCCESS:
                                                                                                                                                      return "partial_success";
                                                                                                                                                  case ChassisController::OperationResult::NETWORK_ERROR:
                                                                                                                                                      return "network_error";
                                                                                                                                                  case ChassisController::OperationResult::TIMEOUT_ERROR:
                                                                                                                                                      return "timeout_error";
                                                                                                                                                  case ChassisController::OperationResult::INVALID_RESPONSE:
                                                                                                                                                      return "invalid_response";
                                                                                                                                                  default:
                                                                                                                                                      return "unknown_error";
                                                                                                                                                  }
                                                                                                                                              }()},
                      {"message", result.message},
                      {"slot_results", slot_results},
                      {"raw_response_hex", TcpClient::binaryToHex(result.raw_response)}}}};

        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully processed chassis reset request for target_ip: {}", target_ip);
    }
    catch (const json::parse_error &e)
    {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_chassis_reset: {}", e.what());
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to execute chassis reset\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_chassis_reset: {}", e.what());
    }
}

void HttpServer::handle_chassis_power_off(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!m_chassis_controller)
        {
            res.set_content("{\"error\":\"Chassis controller not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Chassis controller not available for power-off request");
            return;
        }

        json body = json::parse(req.body);

        // 验证必需的字段
        if (!body.contains("target_ip") || !body["target_ip"].is_string())
        {
            res.set_content("{\"error\":\"'target_ip' field is required and must be a string\"}", "application/json");
            res.status = 400;
            return;
        }

        std::string target_ip = body["target_ip"].get<std::string>();
        int req_id = body.value("request_id", 0);

        ChassisController::OperationResponse result;

        if (body.contains("slots") && body["slots"].is_array())
        {
            // 多槽位操作
            std::vector<int> slot_numbers;
            for (const auto &slot : body["slots"])
            {
                if (slot.is_number_integer())
                {
                    slot_numbers.push_back(slot.get<int>());
                }
            }

            if (slot_numbers.empty())
            {
                res.set_content("{\"error\":\"No valid slot numbers provided\"}", "application/json");
                res.status = 400;
                return;
            }

            result = m_chassis_controller->powerOffChassisBoards(target_ip, slot_numbers, req_id);
        }
        else if (body.contains("slot") && body["slot"].is_number_integer())
        {
            // 单槽位操作
            int slot_number = body["slot"].get<int>();
            result = m_chassis_controller->powerOffChassisBoard(target_ip, slot_number, req_id);
        }
        else
        {
            res.set_content("{\"error\":\"Either 'slot' (integer) or 'slots' (array) field is required\"}", "application/json");
            res.status = 400;
            return;
        }

        // 构建响应（复用reset的响应格式）
        json slot_results = json::array();
        for (const auto &slot_result : result.slot_results)
        {
            json slot_json = {
                {"slot_number", slot_result.slot_number},
                {"status", static_cast<int>(slot_result.status)},
                {"status_text", [&]()
                 {
                     switch (slot_result.status)
                     {
                     case ChassisController::SlotStatus::SUCCESS:
                         return "success";
                     case ChassisController::SlotStatus::FAILED:
                         return "failed";
                     case ChassisController::SlotStatus::REQUEST_OPERATION:
                         return "requested";
                     default:
                         return "no_operation";
                     }
                 }()}};
            slot_results.push_back(slot_json);
        }

        json response = {
            {"api_version", 1},
            {"status", "success"},
            {"data", {{"operation", "power_off"}, {"target_ip", target_ip}, {"request_id", req_id}, {"result", static_cast<int>(result.result)}, {"result_text", [&]()
                                                                                                                                                  {
                                                                                                                                                      switch (result.result)
                                                                                                                                                      {
                                                                                                                                                      case ChassisController::OperationResult::SUCCESS:
                                                                                                                                                          return "success";
                                                                                                                                                      case ChassisController::OperationResult::PARTIAL_SUCCESS:
                                                                                                                                                          return "partial_success";
                                                                                                                                                      case ChassisController::OperationResult::NETWORK_ERROR:
                                                                                                                                                          return "network_error";
                                                                                                                                                      case ChassisController::OperationResult::TIMEOUT_ERROR:
                                                                                                                                                          return "timeout_error";
                                                                                                                                                      case ChassisController::OperationResult::INVALID_RESPONSE:
                                                                                                                                                          return "invalid_response";
                                                                                                                                                      default:
                                                                                                                                                          return "unknown_error";
                                                                                                                                                      }
                                                                                                                                                  }()},
                      {"message", result.message},
                      {"slot_results", slot_results},
                      {"raw_response_hex", TcpClient::binaryToHex(result.raw_response)}}}};

        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully processed chassis power-off request for target_ip: {}", target_ip);
    }
    catch (const json::parse_error &e)
    {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_chassis_power_off: {}", e.what());
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to execute chassis power-off\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_chassis_power_off: {}", e.what());
    }
}

void HttpServer::handle_chassis_power_on(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!m_chassis_controller)
        {
            res.set_content("{\"error\":\"Chassis controller not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Chassis controller not available for power-on request");
            return;
        }

        json body = json::parse(req.body);

        // 验证必需的字段
        if (!body.contains("target_ip") || !body["target_ip"].is_string())
        {
            res.set_content("{\"error\":\"'target_ip' field is required and must be a string\"}", "application/json");
            res.status = 400;
            return;
        }

        std::string target_ip = body["target_ip"].get<std::string>();
        int req_id = body.value("request_id", 0);

        ChassisController::OperationResponse result;

        if (body.contains("slots") && body["slots"].is_array())
        {
            // 多槽位操作
            std::vector<int> slot_numbers;
            for (const auto &slot : body["slots"])
            {
                if (slot.is_number_integer())
                {
                    slot_numbers.push_back(slot.get<int>());
                }
            }

            if (slot_numbers.empty())
            {
                res.set_content("{\"error\":\"No valid slot numbers provided\"}", "application/json");
                res.status = 400;
                return;
            }

            result = m_chassis_controller->powerOnChassisBoards(target_ip, slot_numbers, req_id);
        }
        else if (body.contains("slot") && body["slot"].is_number_integer())
        {
            // 单槽位操作
            int slot_number = body["slot"].get<int>();
            result = m_chassis_controller->powerOnChassisBoard(target_ip, slot_number, req_id);
        }
        else
        {
            res.set_content("{\"error\":\"Either 'slot' (integer) or 'slots' (array) field is required\"}", "application/json");
            res.status = 400;
            return;
        }

        // 构建响应（复用reset的响应格式）
        json slot_results = json::array();
        for (const auto &slot_result : result.slot_results)
        {
            json slot_json = {
                {"slot_number", slot_result.slot_number},
                {"status", static_cast<int>(slot_result.status)},
                {"status_text", [&]()
                 {
                     switch (slot_result.status)
                     {
                     case ChassisController::SlotStatus::SUCCESS:
                         return "success";
                     case ChassisController::SlotStatus::FAILED:
                         return "failed";
                     case ChassisController::SlotStatus::REQUEST_OPERATION:
                         return "requested";
                     default:
                         return "no_operation";
                     }
                 }()}};
            slot_results.push_back(slot_json);
        }

        json response = {
            {"api_version", 1},
            {"status", "success"},
            {"data", {{"operation", "power_on"}, {"target_ip", target_ip}, {"request_id", req_id}, {"result", static_cast<int>(result.result)}, {"result_text", [&]()
                                                                                                                                                 {
                                                                                                                                                     switch (result.result)
                                                                                                                                                     {
                                                                                                                                                     case ChassisController::OperationResult::SUCCESS:
                                                                                                                                                         return "success";
                                                                                                                                                     case ChassisController::OperationResult::PARTIAL_SUCCESS:
                                                                                                                                                         return "partial_success";
                                                                                                                                                     case ChassisController::OperationResult::NETWORK_ERROR:
                                                                                                                                                         return "network_error";
                                                                                                                                                     case ChassisController::OperationResult::TIMEOUT_ERROR:
                                                                                                                                                         return "timeout_error";
                                                                                                                                                     case ChassisController::OperationResult::INVALID_RESPONSE:
                                                                                                                                                         return "invalid_response";
                                                                                                                                                     default:
                                                                                                                                                         return "unknown_error";
                                                                                                                                                     }
                                                                                                                                                 }()},
                      {"message", result.message},
                      {"slot_results", slot_results},
                      {"raw_response_hex", TcpClient::binaryToHex(result.raw_response)}}}};

        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully processed chassis power-on request for target_ip: {}", target_ip);
    }
    catch (const json::parse_error &e)
    {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_chassis_power_on: {}", e.what());
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"error\":\"Failed to execute chassis power-on\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_chassis_power_on: {}", e.what());
    }
}