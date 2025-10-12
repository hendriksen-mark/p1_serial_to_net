#include "ota_server.h"
#include "custom_log.h"
#include <base64.h>
#include <Updater.h>

// Global variables
OTAState otaState = OTA_IDLE;
unsigned long otaStartTime = 0;
unsigned long otaBytesReceived = 0;
unsigned long otaTotalBytes = 0;

// Statistics
unsigned long otaLastUpdate = 0;
unsigned long totalOTAAttempts = 0;
unsigned long successfulOTAUpdates = 0;
unsigned long lastOTAUpdate = 0;


void initializeOTAServer() {
	if (OTA_ENABLED) {
		// OTA is now integrated into HTTP server on port 80
		// This standalone server is no longer used
		REMOTE_LOG_INFO("OTA functionality integrated into HTTP server (port 80)");
		REMOTE_LOG_WARN("OTA enabled - Change default credentials in config.h!");
	}
}

bool verifyOTACredentials(const String& authHeader) {
	// Extract base64 encoded credentials
	// Format: "Authorization: Basic YWRtaW46dXBkYXRlMTIz"
	int basicIndex = authHeader.indexOf("Basic ");
	if (basicIndex == -1) {
		REMOTE_LOG_DEBUG("Auth header format invalid - 'Basic ' not found");
		return false;
	}

	String credentials = authHeader.substring(basicIndex + 6); // Skip "Basic "
	credentials.trim(); // Remove any trailing whitespace

	// Create expected credentials
	String expected = String(OTA_USERNAME) + ":" + String(OTA_PASSWORD);
	String expectedEncoded = base64::encode(expected);

	REMOTE_LOG_DEBUG(("Expected credentials: " + expected).c_str());
	REMOTE_LOG_DEBUG(("Expected encoded: " + expectedEncoded).c_str());
	REMOTE_LOG_DEBUG(("Received encoded: " + credentials).c_str());

	return credentials.equals(expectedEncoded);
}

void sendOTAResponse(EthernetClient& client, int statusCode, const String& message) {
	client.print("HTTP/1.1 ");
	client.print(statusCode);
	client.print(" ");

	switch (statusCode) {
		case 200: client.println("OK"); break;
		case 401: client.println("Unauthorized"); break;
		case 404: client.println("Not Found"); break;
		case 413: client.println("Payload Too Large"); break;
		case 500: client.println("Internal Server Error"); break;
		default: client.println("Unknown"); break;
	}

	client.println("Content-Type: text/plain");
	client.println("Connection: close");
	client.println();
	client.println(message);
}

void sendUploadPage(EthernetClient& client) {
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/html");
	client.println("Connection: close");
	client.println();

	// Simple HTML upload form
	client.println("<!DOCTYPE html>");
	client.println("<html><head><title>P1 Bridge OTA Update</title>");
	client.println("<style>");
	client.println("body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; }");
	client.println(".container { background: #f5f5f5; padding: 20px; border-radius: 10px; }");
	client.println("input[type=file] { margin: 10px 0; }");
	client.println("input[type=submit] { background: #007cba; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }");
	client.println("input[type=submit]:hover { background: #005a87; }");
	client.println(".warning { background: #fff3cd; border: 1px solid #ffeaa7; padding: 10px; border-radius: 5px; margin: 10px 0; }");
	client.println("</style></head><body>");

	client.println("<div class='container'>");
	client.println("<h1>P1 Serial-to-Network Bridge</h1>");
	client.println("<h2>Firmware Update</h2>");

	client.println("<div class='warning'>");
	client.println("<strong>⚠️ WARNING:</strong> Only upload firmware files (.uf2) specifically built for this device. ");
	client.println("Uploading incorrect firmware may brick the device!");
	client.println("</div>");

	client.println("<form method='POST' action='/upload' enctype='multipart/form-data'>");
	client.println("<p>Select firmware file (.uf2):</p>");
	client.println("<input type='file' name='firmware' accept='.uf2' required>");
	client.println("<br><br>");
	client.println("<input type='submit' value='Upload Firmware'>");
	client.println("</form>");

	// Status information
	client.println("<h3>Current Status</h3>");
	client.println("<p>Uptime: " + String(millis() / 1000) + " seconds</p>");
	client.println("<p>Total OTA attempts: " + String(totalOTAAttempts) + "</p>");
	client.println("<p>Successful updates: " + String(successfulOTAUpdates) + "</p>");
	if (lastOTAUpdate > 0) {
		client.println("<p>Last update: " + String((millis() - lastOTAUpdate) / 1000) + " seconds ago</p>");
	}

	client.println("<p><a href='/status'>Check Status</a> | <a href='/'>Refresh</a></p>");
	client.println("</div></body></html>");
}

void handleOTAUpload(EthernetClient& client) {
	REMOTE_LOG_INFO("OTA: Starting firmware upload");

	otaState = OTA_RECEIVING;
	otaStartTime = millis();
	otaBytesReceived = 0;

	// Skip HTTP headers to find the firmware data
	String line;
	String contentLengthStr = "";
	bool foundContentLength = false;
	
	// Read headers
	while (client.connected() && client.available()) {
		line = client.readStringUntil('\n');
		line.trim();
		
		if (line.startsWith("Content-Length: ")) {
			contentLengthStr = line.substring(16);
			foundContentLength = true;
		}
		
		if (line.length() == 0) {
			// Empty line indicates end of headers
			break;
		}
	}

	// Skip multipart boundary and headers
	bool foundBinaryData = false;
	while (client.connected() && client.available() && !foundBinaryData) {
		line = client.readStringUntil('\n');
		line.trim();
		
		if (line.indexOf("Content-Type: application/octet-stream") >= 0 || 
		    line.indexOf("filename=") >= 0) {
			// Skip one more line (empty line after content-type)
			client.readStringUntil('\n');
			foundBinaryData = true;
			break;
		}
	}

	if (!foundBinaryData) {
		sendOTAResponse(client, 400, "Invalid firmware upload format");
		resetOTAState();
		return;
	}

	// Start the updater (RP2040 needs size parameter)
	if (!Update.begin(OTA_MAX_FILE_SIZE)) {
		sendOTAResponse(client, 500, "Failed to start firmware update");
		REMOTE_LOG_ERROR("OTA: Failed to start updater");
		resetOTAState();
		return;
	}

	REMOTE_LOG_INFO("OTA: Updater started, receiving firmware data...");

	// Read and write firmware data
	unsigned long timeout = millis() + OTA_TIMEOUT;
	uint8_t buffer[OTA_BUFFER_SIZE];
	
	while (client.connected() && millis() < timeout) {
		if (client.available()) {
			size_t bytesRead = client.readBytes(buffer, sizeof(buffer));
			
			if (bytesRead > 0) {
				otaBytesReceived += bytesRead;
				
				// Check size limit
				if (otaBytesReceived > OTA_MAX_FILE_SIZE) {
					Update.end();
					sendOTAResponse(client, 413, "Firmware file too large");
					REMOTE_LOG_ERROR("OTA: File too large:", otaBytesReceived);
					resetOTAState();
					return;
				}
				
				// Write to updater
				size_t written = Update.write(buffer, bytesRead);
				if (written != bytesRead) {
					Update.end();
					sendOTAResponse(client, 500, "Flash write error");
					REMOTE_LOG_ERROR("OTA: Flash write failed");
					resetOTAState();
					return;
				}
				
				REMOTE_LOG_DEBUG("OTA: Written bytes:", otaBytesReceived);
			}
		} else {
			delay(1);
		}
		
		// Check if we've likely received all data (simple heuristic)
		if (!client.available() && otaBytesReceived > 100000) {
			delay(100); // Give a bit more time
			if (!client.available()) {
				break; // Assume we're done
			}
		}
	}

	if (millis() >= timeout) {
		Update.end();
		sendOTAResponse(client, 500, "Upload timeout");
		REMOTE_LOG_ERROR("OTA: Upload timeout");
		resetOTAState();
		return;
	}

	// Finalize the update
	if (!Update.end(true)) {
		sendOTAResponse(client, 500, "Failed to finalize firmware update");
		REMOTE_LOG_ERROR("OTA: Failed to finalize update");
		resetOTAState();
		return;
	}

	otaState = OTA_SUCCESS;
	successfulOTAUpdates++;
	lastOTAUpdate = millis();

	REMOTE_LOG_INFO("OTA: Upload completed successfully - bytes written:", otaBytesReceived);

	sendOTAResponse(client, 200, "Upload successful! Device will reboot in 3 seconds to apply new firmware.");
	
	delay(3000); // Give time for response to be sent
	REMOTE_LOG_INFO("OTA: Rebooting to apply firmware...");
	
	// Proper reboot for RP2040
	rp2040.reboot();
}

String getOTAStatus() {
	String status = "P1 Bridge OTA Status\n";
	status += "State: ";

	switch (otaState) {
		case OTA_IDLE: status += "Idle"; break;
		case OTA_RECEIVING: status += "Receiving Upload"; break;
		case OTA_FLASHING: status += "Flashing Firmware"; break;
		case OTA_SUCCESS: status += "Last Update Successful"; break;
		case OTA_ERROR: status += "Error"; break;
	}

	status += "\nUptime: " + String(millis() / 1000) + " seconds";
	status += "\nTotal Attempts: " + String(totalOTAAttempts);
	status += "\nSuccessful Updates: " + String(successfulOTAUpdates);

	if (otaState == OTA_RECEIVING) {
		status += "\nBytes Received: " + String(otaBytesReceived);
		status += "\nUpload Progress: " + String((otaBytesReceived * 100) / OTA_MAX_FILE_SIZE) + "%";
	}

	return status;
}

void resetOTAState() {
	otaState = OTA_IDLE;
	otaStartTime = 0;
	otaBytesReceived = 0;
	otaTotalBytes = 0;
}

// Statistics functions
unsigned long getOTARequestCount() {
	return totalOTAAttempts;
}

unsigned long getOTAUpdateCount() {
	return successfulOTAUpdates;
}

unsigned long getLastOTATimestamp() {
	return lastOTAUpdate;
}
