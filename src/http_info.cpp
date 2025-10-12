#include "http_info.h"
#include "ota_server.h"
#include "custom_log.h"
#include "clients.h"
#include "log_server.h"
#include "p1_handler.h"
#include "ntp_client.h"
#include <Ethernet.h>
#include <base64.h>

// Global variables
EthernetServer httpInfoServer(HTTP_INFO_PORT);
unsigned long totalHTTPRequests = 0;

void initializeHTTPInfoServer() {
	if (HTTP_INFO_ENABLED) {
		httpInfoServer.begin();
		REMOTE_LOG_INFO("HTTP info server listening on port:", HTTP_INFO_PORT);
	}
}

void handleHTTPInfoConnections() {
	if (!HTTP_INFO_ENABLED) return;

	EthernetClient client = httpInfoServer.accept();
	if (client) {
		REMOTE_LOG_DEBUG("HTTP info client connected");
		totalHTTPRequests++;
		handleHTTPRequest(client);
		client.stop();
		REMOTE_LOG_DEBUG("HTTP info client disconnected");
	}
}

void handleHTTPRequest(EthernetClient& client) {
	String request = "";
	String method = "";
	String path = "";
	bool isAuthorized = false;

	// Read the HTTP request
	unsigned long timeout = millis() + 5000; // 5 second timeout
	while (client.connected() && millis() < timeout) {
		if (client.available()) {
			String line = client.readStringUntil('\n');
			line.trim();

			if (line.length() == 0) {
				// End of headers
				break;
			}

			// Parse the first line which should contain the HTTP method and path
			if (method.length() == 0 && (line.startsWith("GET ") || line.startsWith("POST ") || line.startsWith("HEAD ") || line.startsWith("PUT ") || line.startsWith("DELETE "))) {
				// Parse the request line: METHOD path HTTP/1.1
				int firstSpace = line.indexOf(' ');
				int secondSpace = line.indexOf(' ', firstSpace + 1);

				if (firstSpace > 0 && secondSpace > firstSpace) {
					method = line.substring(0, firstSpace);
					path = line.substring(firstSpace + 1, secondSpace);
				} else if (firstSpace > 0) {
					// Handle case where there might not be HTTP version
					method = line.substring(0, firstSpace);
					path = line.substring(firstSpace + 1);
					// Remove any trailing whitespace or HTTP version info
					int httpPos = path.indexOf(" HTTP/");
					if (httpPos > 0) {
						path = path.substring(0, httpPos);
					}
				}
				
				// Debug log the parsed line
				REMOTE_LOG_DEBUG(("Parsed request line: " + line).c_str());
				REMOTE_LOG_DEBUG(("Method: [" + method + "], Path: [" + path + "]").c_str());
			}

			// Check for Authorization header (for OTA endpoints)
			if (line.startsWith("Authorization: ")) {
				REMOTE_LOG_DEBUG(("Auth header received: " + line).c_str());
				if (verifyOTACredentials(line)) {
					isAuthorized = true;
					REMOTE_LOG_DEBUG("OTA credentials verified successfully");
				} else {
					REMOTE_LOG_DEBUG("OTA credentials verification failed");
				}
			}
		}
	}

	String logMsg = "HTTP Request: [" + method + "] [" + path + "]";
	REMOTE_LOG_DEBUG(logMsg.c_str());
	
	// Debug: Check if method is empty
	if (method.length() == 0) {
		REMOTE_LOG_DEBUG("Warning: HTTP method is empty!");
		method = "GET"; // Default to GET if parsing failed
		if (path.length() == 0) {
			path = "/"; // Default to root path
		}
	}

	if (method == "GET") {
		if (path == "/" || path == "/info") {
			sendInfoPage(client);
		} else if (path == "/p1") {
			sendP1DataPage(client);
		} else if (path == "/p1/stream") {
			sendP1DataStream(client);
		} else if (path == "/logs") {
			sendLogsPage(client);
		} else if (path == "/logs/stream") {
			sendLogsDataStream(client);
		} else if (path == "/ota" || path == "/upload") {
			// Handle OTA upload page
			if (isAuthorized) {
				String uploadPage = "<!DOCTYPE html><html><head><title>P1 Bridge OTA Update</title>";
				uploadPage += "<style>";
				uploadPage += "body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background-color: #f5f5f5; }";
				uploadPage += ".container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
				uploadPage += "h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }";
				uploadPage += "input[type=file] { margin: 10px 0; padding: 10px; border: 1px solid #ddd; border-radius: 5px; width: 100%; }";
				uploadPage += "input[type=submit] { background: #e74c3c; color: white; padding: 12px 30px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 10px; }";
				uploadPage += "input[type=submit]:hover { background: #c0392b; }";
				uploadPage += ".warning { background: #fff3cd; border: 1px solid #ffeaa7; padding: 15px; border-radius: 5px; margin: 15px 0; }";
				uploadPage += ".info { background: #d1ecf1; border: 1px solid #bee5eb; padding: 15px; border-radius: 5px; margin: 15px 0; }";
				uploadPage += ".back-link { display: inline-block; margin-top: 20px; padding: 8px 16px; background: #3498db; color: white; text-decoration: none; border-radius: 5px; }";
				uploadPage += ".back-link:hover { background: #2980b9; }";
				uploadPage += "</style></head><body>";
				uploadPage += "<div class='container'>";
				uploadPage += "<h1>P1 Bridge Firmware Update</h1>";
				uploadPage += "<div class='warning'><strong>WARNING:</strong> Only upload firmware files (.bin) specifically built for this device. Uploading incorrect firmware may brick the device!</div>";
				uploadPage += "<div class='info'>";
				uploadPage += "<h3>File Format Requirements</h3>";
				uploadPage += "<p><strong>Required:</strong> Upload the <code>firmware.bin</code> file (NOT .elf or .uf2)</p>";
				uploadPage += "<p><strong>Location:</strong> <code>.pio/build/waveshare_rp2040_zero/firmware.bin</code></p>";
				uploadPage += "<p><strong>Size:</strong> Typical size is 150-200 KB for this project</p>";
				uploadPage += "<p><strong>Build:</strong> Use <code>platformio run</code> to generate firmware.bin</p>";
				uploadPage += "</div>";
				uploadPage += "<form method='POST' action='/upload-firmware' enctype='multipart/form-data'>";
				uploadPage += "<p><strong>Select firmware.bin file:</strong></p>";
				uploadPage += "<input type='file' name='firmware' accept='.bin' required>";
				uploadPage += "<br><input type='submit' value='Upload Firmware'>";
				uploadPage += "</form>";
				uploadPage += "<div class='info'>";
				uploadPage += "<h3>Current Device Status</h3>";
				uploadPage += "<p><strong>Current Time:</strong> " + getFormattedDateTime() + "</p>";
				uploadPage += "<p><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</p>";
				uploadPage += "<p><strong>Flash Size:</strong> 2MB (RP2040)</p>";
				uploadPage += "<p><strong>Max Upload:</strong> " + String(OTA_MAX_FILE_SIZE / 1024) + " KB</p>";
				uploadPage += "<p><strong>Board:</strong> Waveshare RP2040 Zero</p>";
				uploadPage += "<p><strong>Firmware:</strong> P1 Bridge with SSE support</p>";
				uploadPage += "</div>";
				uploadPage += "<a href='/status' class='back-link'>Check OTA Status</a> ";
				uploadPage += "<a href='/' class='back-link'>Main Page</a>";
				uploadPage += "</div></body></html>";
				sendHTTPResponse(client, 200, "text/html", uploadPage);
			} else {
				// Send 401 with proper WWW-Authenticate header
				String headers = "HTTP/1.1 401 Unauthorized\r\n";
				headers += "WWW-Authenticate: Basic realm=\"OTA Access\"\r\n";
				headers += "Content-Type: text/html\r\n";
				headers += "Connection: close\r\n";
				headers += "Server: P1-Bridge/1.0\r\n";
				headers += "\r\n";
				
				String authPage = "<!DOCTYPE html><html><head><title>401 Unauthorized</title></head>";
				authPage += "<body><h1>401 Unauthorized</h1><p>Authentication required for OTA access.</p>";
				authPage += "<p>Please use credentials: <strong>admin</strong> / <strong>update123</strong></p>";
				authPage += "<p><a href='/'>Back to main page</a></p></body></html>";
				
				client.print(headers);
				client.print(authPage);
			}
		} else if (path == "/status") {
			// Handle OTA status directly - simplified for debugging
			String status = "OTA Status: Ready\nUptime: " + String(millis() / 1000) + " seconds\nFirmware: P1 Bridge v1.0";
			sendHTTPResponse(client, 200, "text/plain", status);
		} else {
			// 404 Not Found
			String content = "<!DOCTYPE html><html><head><title>404 Not Found</title></head>";
			content += "<body><h1>404 Not Found</h1><p>The requested resource was not found.</p>";
			content += "<p><a href=\"/\">Return to main page</a></p></body></html>";
			sendHTTPResponse(client, 404, "text/html", content);
		}
	} else if (method == "POST") {
		if (path == "/upload" || path == "/upload-firmware") {
			// Handle OTA upload
			if (isAuthorized) {
				handleOTAUpload(client);
			} else {
				String headers = "HTTP/1.1 401 Unauthorized\r\n";
				headers += "WWW-Authenticate: Basic realm=\"OTA Upload\"\r\n";
				headers += "Content-Type: text/html\r\n";
				headers += "Connection: close\r\n";
				headers += "\r\n";
				
				String authPage = "<!DOCTYPE html><html><head><title>401 Unauthorized</title></head>";
				authPage += "<body><h1>401 Unauthorized</h1><p>Authentication required for OTA upload.</p>";
				authPage += "<p>Please use credentials: <strong>admin</strong> / <strong>update123</strong></p></body></html>";
				
				client.print(headers);
				client.print(authPage);
			}
		} else {
			sendHTTPResponse(client, 404, "text/html", "Not Found");
		}
	} else {
		// Handle unsupported methods or parsing errors - default to showing main page
		REMOTE_LOG_DEBUG(("Unsupported method or parsing error. Method: [" + method + "], treating as GET /").c_str());
		sendInfoPage(client);
	}
}

void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content) {
	// Send headers without Content-Length to avoid mismatch issues
	String headers = "HTTP/1.1 " + String(statusCode) + " OK\r\n";
	headers += "Content-Type: " + contentType + "\r\n";
	headers += "Connection: close\r\n";
	headers += "Server: P1-Bridge/1.0\r\n";
	headers += "\r\n";
	
	client.print(headers);
	client.flush(); // Ensure headers are sent
	
	// Send content in smaller chunks to avoid buffer issues
	const int chunkSize = 512;
	int contentLen = content.length();
	for (int i = 0; i < contentLen; i += chunkSize) {
		String chunk = content.substring(i, min(i + chunkSize, contentLen));
		client.print(chunk);
		client.flush();
		delay(1); // Small delay to prevent overwhelming the W5500
	}
}

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
	sendHTTPResponse(client, 200, "text/html", content);
}

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
	sendHTTPResponse(client, 200, "text/html", content);
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

void sendInfoPage(EthernetClient& client) {
	String content = getDeviceInfoHTML();
	sendHTTPResponse(client, 200, "text/html", content);
}

void sendRedirect(EthernetClient& client, const String& location) {
	String headers = "HTTP/1.1 302 Found\r\n";
	headers += "Location: " + location + "\r\n";
	headers += "Connection: close\r\n";
	headers += "\r\n";
	client.print(headers);
}

String getDeviceInfoHTML() {
	String html = "<!DOCTYPE html>\n";
	html += "<html lang=\"en\">\n";
	html += "<head>\n";
	html += "    <meta charset=\"UTF-8\">\n";
	html += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
	html += "    <title>" + String(HTTP_INFO_TITLE) + "</title>\n";
	html += "    <style>\n";
	html += "        body { font-family: Arial, sans-serif; margin: 40px; background-color: #f5f5f5; }\n";
	html += "        .container { max-width: 800px; margin: 0 auto; background-color: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n";
	html += "        h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }\n";
	html += "        h2 { color: #34495e; margin-top: 30px; }\n";
	html += "        .info-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin: 20px 0; }\n";
	html += "        .info-card { background-color: #ecf0f1; padding: 15px; border-radius: 5px; }\n";
	html += "        .info-label { font-weight: bold; color: #2c3e50; }\n";
	html += "        .services { margin: 20px 0; }\n";
	html += "        .service-link { display: inline-block; margin: 10px; padding: 12px 20px; background-color: #3498db; color: white; text-decoration: none; border-radius: 5px; transition: background-color 0.3s; }\n";
	html += "        .service-link:hover { background-color: #2980b9; }\n";
	html += "        .service-link.upload { background-color: #e74c3c; }\n";
	html += "        .service-link.upload:hover { background-color: #c0392b; }\n";
	html += "        .service-link.logs { background-color: #f39c12; }\n";
	html += "        .service-link.logs:hover { background-color: #d68910; }\n";
	html += "        .footer { margin-top: 30px; text-align: center; color: #7f8c8d; font-size: 0.9em; }\n";
	html += "    </style>\n";
	html += "</head>\n";
	html += "<body>\n";
	html += "    <div class=\"container\">\n";
	html += "        <h1>" + String(HTTP_INFO_TITLE) + "</h1>\n";

	// Device Information
	html += "        <h2>Device Information</h2>\n";
	html += "        <div class=\"info-grid\">\n";
	html += "            <div class=\"info-card\">\n";
	html += "                <div class=\"info-label\">IP Address:</div>\n";
	html += "                <div>" + Ethernet.localIP().toString() + "</div>\n";
	html += "            </div>\n";
	html += "            <div class=\"info-card\">\n";
	html += "                <div class=\"info-label\">MAC Address:</div>\n";
	html += "                <div>";
	byte mac[6];
	Ethernet.MACAddress(mac);
	for (int i = 0; i < 6; i++) {
		if (i > 0) html += ":";
		if (mac[i] < 16) html += "0";
		html += String(mac[i], HEX);
	}
	html += "</div>\n";
	html += "            </div>\n";
	html += "            <div class=\"info-card\">\n";
	html += "                <div class=\"info-label\">Current Time:</div>\n";
	html += "                <div>" + getFormattedDateTime() + " " + (isNTPTimeValid() ? "(NTP)" : "(no sync)") + "</div>\n";
	html += "            </div>\n";
	html += "            <div class=\"info-card\">\n";
	html += "                <div class=\"info-label\">Uptime:</div>\n";
	html += "                <div>" + String(millis() / 1000) + " seconds</div>\n";
	html += "            </div>\n";
	html += "            <div class=\"info-card\">\n";
	html += "                <div class=\"info-label\">Firmware:</div>\n";
	html += "                <div>P1 Bridge v1.0</div>\n";
	html += "            </div>\n";
	html += "        </div>\n";

	// Services
	html += "        <h2>Available Services</h2>\n";
	html += "        <div class=\"services\">\n";
	html += "            <a href=\"/p1\" class=\"service-link\">P1 Data Stream (Port " + String(SERVER_PORT) + ")</a>\n";
	html += "            <a href=\"/logs\" class=\"service-link logs\">Remote Logging (Port " + String(LOG_SERVER_PORT) + ")</a>\n";
	if (OTA_ENABLED) {
		html += "            <a href=\"/ota\" class=\"service-link upload\">Firmware Update</a>\n";
		html += "            <a href=\"/status\" class=\"service-link\">OTA Status</a>\n";
	}
	html += "        </div>\n";

	// Service Descriptions
	html += "        <h2>Service Descriptions</h2>\n";
	html += "        <div class=\"info-card\">\n";
	html += "            <p><strong>P1 Data Stream:</strong> Connect to port " + String(SERVER_PORT) + " with a TCP client to receive real-time P1 smart meter data.</p>\n";
	html += "            <p><strong>Remote Logging:</strong> Connect to port " + String(LOG_SERVER_PORT) + " to monitor system logs and debugging information.</p>\n";
	if (OTA_ENABLED) {
		html += "            <p><strong>Firmware Update:</strong> Access the OTA (Over-The-Air) firmware update interface at /ota. Authentication required.</p>\n";
		html += "            <p><strong>OTA Status:</strong> View OTA update statistics and current status at /status without authentication.</p>\n";
	}
	html += "        </div>\n";

	// Connection Information
	html += "        <h2>Connection Information</h2>\n";
	html += "        <div class=\"info-card\">\n";
	html += "            <p><strong>TCP Connections:</strong></p>\n";
	html += "            <ul>\n";
	html += "                <li>P1 Data: <code>telnet " + Ethernet.localIP().toString() + " " + String(SERVER_PORT) + "</code></li>\n";
	html += "                <li>Logs: <code>telnet " + Ethernet.localIP().toString() + " " + String(LOG_SERVER_PORT) + "</code></li>\n";
	html += "            </ul>\n";
	html += "            <p><strong>W5500 Socket Usage:</strong> P1:" + String(MAX_CONNECTIONS) + " + Log:" + String(MAX_LOG_CONNECTIONS) + " + HTTP/OTA:1 + NTP:0 + DHCP:1 = 7/8 sockets</p>\n";
	html += "        </div>\n";

	// Footer
	html += "        <div class=\"footer\">\n";
	html += "            <p>P1 Serial-to-Network Bridge | " + getFormattedDateTime() + " | Uptime: " + String(millis() / 1000) + "s | Requests: " + String(totalHTTPRequests) + "</p>\n";
	html += "        </div>\n";
	html += "    </div>\n";
	html += "</body>\n";
	html += "</html>\n";

	return html;
}

String getHTTPHeaders(int statusCode, const String& contentType, int contentLength) {
	String statusText;
	switch (statusCode) {
		case 200: statusText = "OK"; break;
		case 302: statusText = "Found"; break;
		case 404: statusText = "Not Found"; break;
		case 405: statusText = "Method Not Allowed"; break;
		default: statusText = "Unknown"; break;
	}

	String headers = "HTTP/1.1 " + String(statusCode) + " " + statusText + "\r\n";
	headers += "Content-Type: " + contentType + "\r\n";

	if (contentLength >= 0) {
		headers += "Content-Length: " + String(contentLength) + "\r\n";
	}

	headers += "Connection: close\r\n";
	headers += "Server: P1-Bridge/1.0\r\n";
	headers += "\r\n";

	return headers;
}

unsigned long getHTTPRequestCount() {
	return totalHTTPRequests;
}