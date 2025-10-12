#include "http_info.h"
#include "custom_log.h"
#include <Ethernet.h>

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

			if (line.startsWith("GET ") || line.startsWith("POST ")) {
				// Parse the request line: METHOD path HTTP/1.1
				int firstSpace = line.indexOf(' ');
				int secondSpace = line.indexOf(' ', firstSpace + 1);

				if (firstSpace > 0 && secondSpace > firstSpace) {
					method = line.substring(0, firstSpace);
					path = line.substring(firstSpace + 1, secondSpace);
				}
			}
		}
	}

	String logMsg = "HTTP Request: " + method + " " + path;
	REMOTE_LOG_DEBUG(logMsg.c_str());

	if (method == "GET") {
		if (path == "/" || path == "/info") {
			sendInfoPage(client);
		} else if (path == "/p1") {
			sendRedirect(client, "http://" + Ethernet.localIP().toString() + ":" + String(SERVER_PORT));
		} else if (path == "/logs") {
			sendRedirect(client, "http://" + Ethernet.localIP().toString() + ":" + String(LOG_SERVER_PORT));
		} else if (path == "/ota" || path == "/upload") {
			sendRedirect(client, "http://" + Ethernet.localIP().toString() + ":" + String(OTA_SERVER_PORT));
		} else if (path == "/status") {
			sendRedirect(client, "http://" + Ethernet.localIP().toString() + ":" + String(OTA_SERVER_PORT) + "/status");
		} else {
			// 404 Not Found
			String content = "<!DOCTYPE html><html><head><title>404 Not Found</title></head>";
			content += "<body><h1>404 Not Found</h1><p>The requested resource was not found.</p>";
			content += "<p><a href=\"/\">Return to main page</a></p></body></html>";
			sendHTTPResponse(client, 404, "text/html", content);
		}
	} else {
		// Method not allowed
		String content = "<!DOCTYPE html><html><head><title>405 Method Not Allowed</title></head>";
		content += "<body><h1>405 Method Not Allowed</h1>";
		content += "<p>Only GET requests are supported.</p></body></html>";
		sendHTTPResponse(client, 405, "text/html", content);
	}
}

void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content) {
	String headers = getHTTPHeaders(statusCode, contentType, content.length());
	client.print(headers);
	client.print(content);
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
	html += "            <a href=\"/p1\" class=\"service-link\">📊 P1 Data Stream (Port " + String(SERVER_PORT) + ")</a>\n";
	html += "            <a href=\"/logs\" class=\"service-link logs\">📋 Remote Logging (Port " + String(LOG_SERVER_PORT) + ")</a>\n";
	if (OTA_ENABLED) {
		html += "            <a href=\"/ota\" class=\"service-link upload\">🚀 Firmware Update (Port " + String(OTA_SERVER_PORT) + ")</a>\n";
		html += "            <a href=\"/status\" class=\"service-link\">📈 OTA Status</a>\n";
	}
	html += "        </div>\n";

	// Service Descriptions
	html += "        <h2>Service Descriptions</h2>\n";
	html += "        <div class=\"info-card\">\n";
	html += "            <p><strong>P1 Data Stream:</strong> Connect to port " + String(SERVER_PORT) + " with a TCP client to receive real-time P1 smart meter data.</p>\n";
	html += "            <p><strong>Remote Logging:</strong> Connect to port " + String(LOG_SERVER_PORT) + " to monitor system logs and debugging information.</p>\n";
	if (OTA_ENABLED) {
		html += "            <p><strong>Firmware Update:</strong> Access the OTA (Over-The-Air) firmware update interface on port " + String(OTA_SERVER_PORT) + ". Authentication required.</p>\n";
		html += "            <p><strong>OTA Status:</strong> View OTA update statistics and current status without authentication.</p>\n";
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
	html += "        </div>\n";

	// Footer
	html += "        <div class=\"footer\">\n";
	html += "            <p>P1 Serial-to-Network Bridge | Uptime: " + String(millis() / 1000) + "s | Requests: " + String(totalHTTPRequests) + "</p>\n";
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