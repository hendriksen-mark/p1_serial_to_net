#include "web/status_web_handler.h"
#include "ota_server.h"
#include "custom_log.h"
#include "config.h"
#include "ntp_client.h"
#include <Ethernet.h>

// Extern declarations for global variables used from main
extern unsigned long totalHTTPRequests;
extern bool isNTPTimeValid();

void sendInfoPage(EthernetClient& client) {
	String content = getDeviceInfoHTML();
	// Send response using HTTP server utility
	extern void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content);
	sendHTTPResponse(client, 200, "text/html", content);
}

void handleStatusPage(EthernetClient& client) {
	// Handle OTA status directly - simplified for debugging
	String status = "OTA Status: Ready\nUptime: " + String(millis() / 1000) + " seconds\nFirmware: P1 Bridge v1.0";
	extern void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content);
	sendHTTPResponse(client, 200, "text/plain", status);
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