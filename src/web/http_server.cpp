#include "web/http_server.h"
#include "web/p1_web_handler.h"
#include "web/logs_web_handler.h"
#include "web/status_web_handler.h"
#include "ota_server.h"
#include "custom_log.h"
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

	// Route requests to appropriate handlers
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
			if (isAuthorized) {
				// Generate OTA upload page
				String uploadPage = generateOTAUploadPage();
				sendHTTPResponse(client, 200, "text/html", uploadPage);
			} else {
				sendUnauthorizedResponse(client, "OTA Access");
			}
		} else if (path == "/status") {
			handleStatusPage(client);
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
				sendUnauthorizedResponse(client, "OTA Upload");
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

void sendUnauthorizedResponse(EthernetClient& client, const String& realm) {
	String headers = "HTTP/1.1 401 Unauthorized\r\n";
	headers += "WWW-Authenticate: Basic realm=\"" + realm + "\"\r\n";
	headers += "Content-Type: text/html\r\n";
	headers += "Connection: close\r\n";
	headers += "Server: P1-Bridge/1.0\r\n";
	headers += "\r\n";
	
	String authPage = "<!DOCTYPE html><html><head><title>401 Unauthorized</title></head>";
	authPage += "<body><h1>401 Unauthorized</h1><p>Authentication required for " + realm + " access.</p>";
	authPage += "<p>Please use credentials: <strong>admin</strong> / <strong>update123</strong></p>";
	authPage += "<p><a href='/'>Back to main page</a></p></body></html>";
	
	client.print(headers);
	client.print(authPage);
}

String getHTTPHeaders(int statusCode, const String& contentType, int contentLength) {
	String headers = "HTTP/1.1 " + String(statusCode);
	
	switch (statusCode) {
		case 200: headers += " OK"; break;
		case 404: headers += " Not Found"; break;
		case 401: headers += " Unauthorized"; break;
		case 500: headers += " Internal Server Error"; break;
		default: headers += " Unknown"; break;
	}
	
	headers += "\r\n";
	headers += "Content-Type: " + contentType + "\r\n";
	headers += "Connection: close\r\n";
	headers += "Server: P1-Bridge/1.0\r\n";
	
	if (contentLength > 0) {
		headers += "Content-Length: " + String(contentLength) + "\r\n";
	}
	
	headers += "\r\n";
	return headers;
}

unsigned long getHTTPRequestCount() {
	return totalHTTPRequests;
}

String generateOTAUploadPage() {
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
	return uploadPage;
}