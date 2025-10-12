#include "web/p1_web_handler.h"
#include "custom_log.h"
#include "config.h"
#include "ntp_client.h"
#include <Ethernet.h>

// Extern declarations for global variables used from main
extern String p1Buffer;
extern bool p1MessageComplete;
extern unsigned long lastP1DataReceived;
extern unsigned long totalP1Messages;
extern unsigned long totalBytesReceived;
extern unsigned long getConnectedClientCount();
extern bool isNTPTimeValid();

void sendP1DataPage(EthernetClient& client) {
	String content = "<!DOCTYPE html><html><head>";
	content += "<title>P1 Data Stream - P1 Serial Bridge</title>";
	content += "<meta charset='UTF-8'>";
	// Removed auto-refresh meta tag - now using SSE
	content += "<style>";
	content += "body { font-family: 'Courier New', monospace; background: #1a1a1a; color: #00ff00; margin: 20px; }";
	content += ".container { max-width: 1000px; margin: 0 auto; background: #000; padding: 20px; border-radius: 8px; border: 1px solid #333; }";
	content += "h1 { color: #00ff00; text-align: center; margin-bottom: 20px; }";
	content += ".status { background: #333; padding: 10px; margin: 10px 0; border-radius: 5px; }";
	content += ".data-stream { background: #111; padding: 15px; border: 1px solid #333; border-radius: 5px; font-size: 12px; white-space: pre-wrap; }";
	content += ".nav-link { display: inline-block; background: #333; color: #00ff00; padding: 8px 15px; text-decoration: none; margin: 5px; border-radius: 5px; }";
	content += ".nav-link:hover { background: #555; }";
	content += ".info { color: #ffff00; margin: 10px 0; }";
	content += ".connection-status { color: #00ff00; font-size: 11px; margin-left: 10px; }";
	content += ".online { color: #00ff00; }";
	content += ".offline { color: #ff0000; }";
	content += "</style>";
	content += "<script>";
	content += "let refreshTimer;";
	content += "let isConnecting = false;";
	content += "function fetchP1Data() {";
	content += "  if(isConnecting) return;";
	content += "  isConnecting = true;";
	content += "  document.getElementById('connection-status').innerHTML = 'Updating...';";
	content += "  document.getElementById('connection-status').className = 'connection-status online';";
	content += "  ";
	content += "  const eventSource = new EventSource('/p1/stream');";
	content += "  const timeout = setTimeout(() => {";
	content += "    eventSource.close();";
	content += "    isConnecting = false;";
	content += "    scheduleNext();";
	content += "  }, 3000);";
	content += "  ";
	content += "  eventSource.addEventListener('p1data', function(e) {";
	content += "    try {";
	content += "      const data = JSON.parse(e.data);";
	content += "      updateP1Display(data);";
	content += "    } catch(err) {";
	content += "      console.error('Error parsing P1 data:', err);";
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
	content += "  // refreshTimer = setTimeout(fetchP1Data, 3000);";
	content += "}";
	content += "function updateP1Display(data) {";
	content += "  if(data.content) document.getElementById('p1-data').innerHTML = data.content;";
	content += "  if(data.status) document.getElementById('status-info').innerHTML = data.status;";
	content += "}";
	content += "function refreshData() { fetchP1Data(); }"; // Manual refresh function
	content += "window.onload = fetchP1Data;";
	content += "window.onbeforeunload = function() { clearTimeout(refreshTimer); };";
	content += "</script></head><body>";
	content += "<div class='container'>";
	content += "<h1>P1 Smart Meter Data Stream</h1>";
	content += "<div class='status' id='status-info'>";
	content += "<strong>Status:</strong> " + String(getConnectedClientCount()) + " clients connected to TCP port " + String(SERVER_PORT);
	content += " | <strong>Current Time:</strong> " + getFormattedDateTime();
	content += " | <strong>Uptime:</strong> " + String(millis() / 1000) + "s";
	content += "</div>";
	content += "<div class='info'>Current P1 smart meter data <span id='connection-status' class='connection-status'>Connecting...</span></div>";
	content += "<div style='margin: 10px 0;'><button onclick='refreshData()' style='background: #21262d; color: #58a6ff; border: 1px solid #30363d; padding: 8px 15px; border-radius: 5px; cursor: pointer;'>Refresh Data</button></div>";
	content += "<div class='data-display' id='p1-data'>";
	// Show recent P1 data from buffer without disrupting TCP clients
	content += "=== P1 DATA DIAGNOSTICS ===\n";
	content += "Current time: " + getFormattedDateTime() + " (" + (isNTPTimeValid() ? "NTP synced" : "no NTP") + ")\n";
	content += "p1MessageComplete: " + String(p1MessageComplete ? "true" : "false") + "\n";
	content += "p1Buffer.length(): " + String(p1Buffer.length()) + "\n";
	content += "totalP1Messages: " + String(totalP1Messages) + "\n";
	content += "totalBytesReceived: " + String(totalBytesReceived) + "\n";
	if (lastP1DataReceived > 0) {
		content += "lastP1DataReceived: " + String((millis() - lastP1DataReceived) / 1000) + " seconds ago\n";
	} else {
		content += "lastP1DataReceived: never\n";
	}
	content += "Connected P1 clients: " + String(getConnectedClientCount()) + "\n";
	content += "System uptime: " + String(millis() / 1000) + " seconds\n\n";
	
	if (p1MessageComplete && p1Buffer.length() > 0) {
		content += "=== LATEST P1 MESSAGE ===\n";
		content += p1Buffer;
	} else if (p1Buffer.length() > 0) {
		content += "=== PARTIAL P1 DATA (incomplete) ===\n";
		content += p1Buffer;
	} else {
		content += "=== NO P1 DATA AVAILABLE ===\n";
		content += "Check:\n";
		content += "1. P1 meter is connected and powered\n";
		content += "2. Serial connection (GPIO0/1 at 115200 baud)\n";
		content += "3. Level shifting circuit (5V -> 3.3V)\n";
		content += "4. P1 data request pin if used\n";
	}
	content += "</div>";
	content += "<div style='text-align: center; margin-top: 20px;'>";
	content += "<a href='/' class='nav-link'>Main Page</a>";
	content += "<a href='/logs' class='nav-link'>View Logs</a>";
	content += "<a href='/status' class='nav-link'>OTA Status</a>";
	content += "</div>";
	content += "<div style='text-align: center; margin-top: 15px; color: #666; font-size: 11px;'>";
	content += "For direct TCP connection: <code style='color: #00ff00;'>telnet " + Ethernet.localIP().toString() + " " + String(SERVER_PORT) + "</code>";
	content += "</div>";
	content += "</div></body></html>";
	
	// Send response using HTTP server utility
	extern void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content);
	sendHTTPResponse(client, 200, "text/html", content);
}

void sendP1DataStream(EthernetClient& client) {
	// Send SSE headers
	String headers = "HTTP/1.1 200 OK\r\n";
	headers += "Content-Type: text/event-stream\r\n";
	headers += "Cache-Control: no-cache\r\n";
	headers += "Connection: keep-alive\r\n";
	headers += "Access-Control-Allow-Origin: *\r\n";
	headers += "\r\n";
	client.print(headers);
	
	// Send initial data and close - non-blocking approach
	String currentP1Data = getCurrentP1DataJSON();
	client.print("event: p1data\n");
	client.print("data: ");
	client.print(currentP1Data);
	client.print("\n\n");
	
	// Send heartbeat with current time
	client.print("event: heartbeat\n");
	client.print("data: {\"timestamp\":\"");
	client.print(getFormattedDateTime());
	client.print("\"}\n\n");
	client.flush();
	
	// Note: We're NOT using a blocking while loop here
	// Instead, we send current data and let the browser reconnect for updates
	// This prevents blocking the main loop which reads P1 data
	
	REMOTE_LOG_DEBUG("SSE: Sent P1 data snapshot, closing connection to prevent blocking");
}

String getCurrentP1DataJSON() {
	String json = "{";
	
	// Build status information
	json += "\"status\":\"";
	json += "<strong>Status:</strong> " + String(getConnectedClientCount()) + " clients connected to TCP port " + String(SERVER_PORT);
	json += " | <strong>Current Time:</strong> " + getFormattedDateTime();
	json += " | <strong>Uptime:</strong> " + String(millis() / 1000) + "s";
	json += "\",";
	
	// Build content with P1 data
	json += "\"content\":\"";
	String content = "=== P1 DATA DIAGNOSTICS ===\\n";
	content += "Current time: " + getFormattedDateTime() + " (" + (isNTPTimeValid() ? "NTP synced" : "no NTP") + ")\\n";
	content += "p1MessageComplete: " + String(p1MessageComplete ? "true" : "false") + "\\n";
	content += "p1Buffer.length(): " + String(p1Buffer.length()) + "\\n";
	content += "totalP1Messages: " + String(totalP1Messages) + "\\n";
	content += "totalBytesReceived: " + String(totalBytesReceived) + "\\n";
	if (lastP1DataReceived > 0) {
		content += "lastP1DataReceived: " + String((millis() - lastP1DataReceived) / 1000) + " seconds ago\\n";
	} else {
		content += "lastP1DataReceived: never\\n";
	}
	content += "Connected P1 clients: " + String(getConnectedClientCount()) + "\\n";
	content += "System uptime: " + String(millis() / 1000) + " seconds\\n\\n";
	
	if (p1MessageComplete && p1Buffer.length() > 0) {
		content += "=== LATEST P1 MESSAGE ===\\n";
		// Escape the P1 data for JSON
		String escapedP1Data = p1Buffer;
		escapedP1Data.replace("\\", "\\\\");
		escapedP1Data.replace("\"", "\\\"");
		escapedP1Data.replace("\r", "\\r");
		escapedP1Data.replace("\n", "\\n");
		content += escapedP1Data;
	} else if (p1Buffer.length() > 0) {
		content += "=== PARTIAL P1 DATA (incomplete) ===\\n";
		String escapedP1Data = p1Buffer;
		escapedP1Data.replace("\\", "\\\\");
		escapedP1Data.replace("\"", "\\\"");
		escapedP1Data.replace("\r", "\\r");
		escapedP1Data.replace("\n", "\\n");
		content += escapedP1Data;
	} else {
		content += "=== NO P1 DATA AVAILABLE ===\\n";
		content += "Check:\\n";
		content += "1. P1 meter is connected and powered\\n";
		content += "2. Serial connection (GPIO0/1 at 115200 baud)\\n";
		content += "3. Level shifting circuit (5V -> 3.3V)\\n";
		content += "4. P1 data request pin if used\\n";
	}
	
	// Escape content for JSON
	content.replace("\"", "\\\"");
	json += content;
	json += "\"";
	
	json += "}";
	return json;
}