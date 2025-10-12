#include "web/logs_web_handler.h"
#include "custom_log.h"
#include "config.h"
#include "ntp_client.h"
#include <Ethernet.h>

// Extern declarations for global variables used from main
extern unsigned long totalP1Messages;
extern unsigned long totalLogMessages;
extern unsigned long totalBytesSent;
extern unsigned long totalHTTPRequests;
extern unsigned long lastP1DataReceived;
extern unsigned long getConnectedLogClientCount();
extern bool isNTPTimeValid();
extern unsigned long getConnectedClientCount();

void sendLogsPage(EthernetClient& client) {
	String content = "<!DOCTYPE html><html><head>";
	content += "<title>Debug Logs - P1 Serial Bridge</title>";
	content += "<meta charset='UTF-8'>";
	// Removed auto-refresh meta tag - now using SSE
	content += "<style>";
	content += "body { font-family: 'Courier New', monospace; background: #0d1117; color: #c9d1d9; margin: 20px; }";
	content += ".container { max-width: 1200px; margin: 0 auto; background: #161b22; padding: 20px; border-radius: 8px; border: 1px solid #30363d; }";
	content += "h1 { color: #58a6ff; text-align: center; margin-bottom: 20px; }";
	content += ".status { background: #21262d; padding: 10px; margin: 10px 0; border-radius: 5px; border: 1px solid #30363d; }";
	content += ".log-stream { background: #0d1117; padding: 15px; border: 1px solid #30363d; border-radius: 5px; font-size: 11px; max-height: 500px; overflow-y: auto; white-space: pre-wrap; }";
	content += ".nav-link { display: inline-block; background: #21262d; color: #58a6ff; padding: 8px 15px; text-decoration: none; margin: 5px; border-radius: 5px; border: 1px solid #30363d; }";
	content += ".nav-link:hover { background: #30363d; }";
	content += ".info { color: #f0883e; margin: 10px 0; }";
	content += ".log-entry { margin: 2px 0; }";
	content += ".debug { color: #7c3aed; }";
	content += ".info { color: #2563eb; }";
	content += ".warning { color: #f59e0b; }";
	content += ".error { color: #ef4444; }";
	content += ".connection-status { color: #58a6ff; font-size: 11px; margin-left: 10px; }";
	content += ".online { color: #2da44e; }";
	content += ".offline { color: #f85149; }";
	content += "</style>";
	content += "<script>";
	content += "let refreshTimer;";
	content += "let isConnecting = false;";
	content += "function fetchLogsData() {";
	content += "  if(isConnecting) return;";
	content += "  isConnecting = true;";
	content += "  document.getElementById('connection-status').innerHTML = 'Updating...';";
	content += "  document.getElementById('connection-status').className = 'connection-status online';";
	content += "  ";
	content += "  const eventSource = new EventSource('/logs/stream');";
	content += "  const timeout = setTimeout(() => {";
	content += "    eventSource.close();";
	content += "    isConnecting = false;";
	content += "    scheduleNext();";
	content += "  }, 3000);";
	content += "  ";
	content += "  eventSource.addEventListener('logsdata', function(e) {";
	content += "    try {";
	content += "      const data = JSON.parse(e.data);";
	content += "      updateLogsDisplay(data);";
	content += "    } catch(err) {";
	content += "      console.error('Error parsing logs data:', err);";
	content += "    }";
	content += "  });";
	content += "  ";
	content += "  eventSource.addEventListener('heartbeat', function(e) {";
	content += "    const data = JSON.parse(e.data);";
	content += "    document.getElementById('connection-status').innerHTML = 'Live - Last update: ' + data.timestamp;";
	content += "    document.getElementById('connection-status').className = 'connection-status online';";
	content += "    clearTimeout(timeout);";
	content += "    eventSource.close();";
	content += "    isConnecting = false;";
	content += "    scheduleNext();";
	content += "  });";
	content += "  ";
	content += "  eventSource.onerror = function(e) {";
	content += "    clearTimeout(timeout);";
	content += "    eventSource.close();";
	content += "    isConnecting = false;";
	content += "    document.getElementById('connection-status').innerHTML = 'Connection error - Retrying...';";
	content += "    document.getElementById('connection-status').className = 'connection-status offline';";
	content += "    scheduleNext();";
	content += "  };";
	content += "}";
	content += "function scheduleNext() {";
	content += "  // Removed automatic refresh - SSE is now manual/on-demand only";
	content += "  // refreshTimer = setTimeout(fetchLogsData, 3000);";
	content += "}";
	content += "function updateLogsDisplay(data) {";
	content += "  if(data.content) document.getElementById('logs-data').innerHTML = data.content;";
	content += "  if(data.status) document.getElementById('status-info').innerHTML = data.status;";
	content += "}";
	content += "function refreshData() { fetchLogsData(); }"; // Manual refresh function
	content += "window.onload = fetchLogsData;";
	content += "window.onbeforeunload = function() { clearTimeout(refreshTimer); };";
	content += "</script></head><body>";
	content += "<div class='container'>";
	content += "<h1>System Debug Logs</h1>";
	content += "<div class='status' id='status-info'>";
	content += "<strong>Status:</strong> " + String(getConnectedLogClientCount()) + " clients connected to TCP port " + String(LOG_SERVER_PORT);
	content += " | <strong>Current Time:</strong> " + getFormattedDateTime();
	content += " | <strong>Log Level:</strong> DEBUG | <strong>Uptime:</strong> " + String(millis() / 1000) + "s";
	content += "</div>";
	content += "<div class='info'>Recent log entries <span id='connection-status' class='connection-status'>Connecting...</span></div>";
	content += "<div style='margin: 10px 0;'><button onclick='refreshData()' style='background: #21262d; color: #58a6ff; border: 1px solid #30363d; padding: 8px 15px; border-radius: 5px; cursor: pointer;'>Refresh Logs</button></div>";
	content += "<div class='log-stream' id='logs-data'>";
	// Show system status and statistics instead of actual log buffer
	String currentTime = getFormattedDateTime();
	content += "[" + currentTime + "] System Status Report<br>";
	content += "[" + currentTime + "] NTP Status: " + String(isNTPTimeValid() ? "Synchronized" : "Not synced") + "<br>";
	content += "[" + currentTime + "] P1 clients connected: " + String(getConnectedClientCount()) + "<br>";
	content += "[" + currentTime + "] Log clients connected: " + String(getConnectedLogClientCount()) + "<br>";
	content += "[" + currentTime + "] Total P1 messages: " + String(totalP1Messages) + "<br>";
	content += "[" + currentTime + "] Total log messages: " + String(totalLogMessages) + "<br>";
	content += "[" + currentTime + "] Total bytes sent: " + String(totalBytesSent) + "<br>";
	content += "[" + currentTime + "] HTTP requests handled: " + String(totalHTTPRequests) + "<br>";
	content += "[" + currentTime + "] System uptime: " + String(millis() / 1000) + " seconds<br>";
	if (lastP1DataReceived > 0) {
		content += "[" + currentTime + "] Last P1 data: " + String((millis() - lastP1DataReceived) / 1000) + " seconds ago<br>";
	}
	content += "</div>";
	content += "<div style='text-align: center; margin-top: 20px;'>";
	content += "<a href='/' class='nav-link'>Main Page</a>";
	content += "<a href='/p1' class='nav-link'>P1 Data</a>";
	content += "<a href='/status' class='nav-link'>OTA Status</a>";
	content += "</div>";
	content += "<div style='text-align: center; margin-top: 15px; color: #6e7681; font-size: 11px;'>";
	content += "For direct TCP connection: <code style='color: #58a6ff;'>telnet " + Ethernet.localIP().toString() + " " + String(LOG_SERVER_PORT) + "</code>";
	content += "</div>";
	content += "</div></body></html>";
	
	// Send response using HTTP server utility
	extern void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content);
	sendHTTPResponse(client, 200, "text/html", content);
}

void sendLogsDataStream(EthernetClient& client) {
	// Send SSE headers
	String headers = "HTTP/1.1 200 OK\r\n";
	headers += "Content-Type: text/event-stream\r\n";
	headers += "Cache-Control: no-cache\r\n";
	headers += "Connection: keep-alive\r\n";
	headers += "Access-Control-Allow-Origin: *\r\n";
	headers += "\r\n";
	client.print(headers);
	
	// Send initial data and close - non-blocking approach
	String currentLogsData = getCurrentLogsDataJSON();
	client.print("event: logsdata\n");
	client.print("data: ");
	client.print(currentLogsData);
	client.print("\n\n");
	
	// Send heartbeat with current time
	client.print("event: heartbeat\n");
	client.print("data: {\"timestamp\":\"");
	client.print(getFormattedDateTime());
	client.print("\"}\n\n");
	client.flush();
	
	// Note: We're NOT using a blocking while loop here
	// This prevents blocking the main loop which reads P1 data
	
	REMOTE_LOG_DEBUG("SSE: Sent logs data snapshot, closing connection to prevent blocking");
}

String getCurrentLogsDataJSON() {
	String json = "{";
	
	// Build status information
	json += "\"status\":\"";
	json += "<strong>Status:</strong> " + String(getConnectedLogClientCount()) + " clients connected to TCP port " + String(LOG_SERVER_PORT);
	json += " | <strong>Current Time:</strong> " + getFormattedDateTime();
	json += " | <strong>Log Level:</strong> DEBUG | <strong>Uptime:</strong> " + String(millis() / 1000) + "s";
	json += "\",";
	
	// Build content with logs data
	json += "\"content\":\"";
	String content = "";
	String currentTime = getFormattedDateTime();
	content += "[" + currentTime + "] System Status Report\\n";
	content += "[" + currentTime + "] NTP Status: " + String(isNTPTimeValid() ? "Synchronized" : "Not synced") + "\\n";
	content += "[" + currentTime + "] P1 clients connected: " + String(getConnectedClientCount()) + "\\n";
	content += "[" + currentTime + "] Log clients connected: " + String(getConnectedLogClientCount()) + "\\n";
	content += "[" + currentTime + "] Total P1 messages: " + String(totalP1Messages) + "\\n";
	content += "[" + currentTime + "] Total log messages: " + String(totalLogMessages) + "\\n";
	content += "[" + currentTime + "] Total bytes sent: " + String(totalBytesSent) + "\\n";
	content += "[" + currentTime + "] HTTP requests handled: " + String(totalHTTPRequests) + "\\n";
	content += "[" + currentTime + "] System uptime: " + String(millis() / 1000) + " seconds\\n";
	if (lastP1DataReceived > 0) {
		content += "[" + currentTime + "] Last P1 data: " + String((millis() - lastP1DataReceived) / 1000) + " seconds ago\\n";
	}
	
	// Escape content for JSON
	content.replace("\"", "\\\"");
	json += content;
	json += "\"";
	
	json += "}";
	return json;
}