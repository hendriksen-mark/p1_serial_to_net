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


bool verifyOTACredentials(const String& authHeader) {
	// Extract base64 encoded credentials
	// Format: "Authorization: Basic YWRtaW46dXBkYXRlMTIz" decodes to "admin:update123"
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

void handleOTAUpload(EthernetClient& client) {
	REMOTE_LOG_INFO("OTA: Starting firmware upload");

	otaState = OTA_RECEIVING;
	otaStartTime = millis();
	otaBytesReceived = 0;

	// Read and parse HTTP headers first
	String line;
	String contentLengthStr = "";
	String boundary = "";
	bool foundContentLength = false;
	
	REMOTE_LOG_DEBUG("OTA: Reading HTTP headers...");
	
	// Read HTTP headers until empty line
	while (client.connected() && client.available()) {
		line = client.readStringUntil('\n');
		line.trim();
		
		REMOTE_LOG_DEBUG("OTA: Header:", line.c_str());
		
		// Check if this line looks like a multipart boundary (starts with ------) 
		// This can happen when boundary appears right after HTTP headers
		if (line.startsWith("------") && boundary.length() == 0) {
			boundary = line.substring(2); // Remove the "--" prefix
			REMOTE_LOG_DEBUG("OTA: Found boundary in header section:", boundary.c_str());
			// Continue reading to find the actual end of headers
			continue;
		}
		
		if (line.startsWith("Content-Length: ")) {
			contentLengthStr = line.substring(16);
			foundContentLength = true;
			REMOTE_LOG_DEBUG("OTA: Content-Length:", contentLengthStr.c_str());
		}
		
		if (line.startsWith("Content-Type:") && line.indexOf("multipart/form-data") >= 0) {
			REMOTE_LOG_DEBUG("OTA: Found multipart content-type:", line.c_str());
			int boundaryIndex = line.indexOf("boundary=");
			if (boundaryIndex >= 0) {
				String headerBoundary = line.substring(boundaryIndex + 9); // Extract boundary after "boundary="
				headerBoundary.trim(); // Remove any whitespace
				if (boundary.length() == 0) {
					boundary = headerBoundary;
				}
				REMOTE_LOG_DEBUG("OTA: Extracted boundary from header:", boundary.c_str());
			}
		}
		
		if (line.length() == 0) {
			// Empty line indicates end of HTTP headers
			REMOTE_LOG_DEBUG("OTA: End of HTTP headers reached");
			break;
		}
	}

	// If no boundary found in headers, try to detect from first line of body
	if (boundary.length() == 0 && client.available()) {
		line = client.readStringUntil('\n');
		line.trim();
		REMOTE_LOG_DEBUG("OTA: First body line:", line.c_str());
		
		// Check if this line is a boundary (starts with --)
		if (line.startsWith("--") && line.length() > 2) {
			boundary = line.substring(2); // Remove the "--" prefix
			REMOTE_LOG_DEBUG("OTA: Extracted boundary from body:", boundary.c_str());
		}
	}

	if (boundary.length() == 0) {
		REMOTE_LOG_ERROR("OTA: No multipart boundary found in headers or body");
		sendOTAResponse(client, 400, "Invalid multipart upload - no boundary");
		resetOTAState();
		return;
	}

	// Since we already found the boundary during header parsing and consumed
	// the multipart headers (Content-Disposition, Content-Type), we can
	// proceed directly to reading the file content
	bool foundFileStart = true;
	
	REMOTE_LOG_DEBUG("OTA: Multipart headers already parsed, ready for file content");

	if (!foundFileStart) {
		REMOTE_LOG_ERROR("OTA: Could not find file content in multipart data");
		sendOTAResponse(client, 400, "Invalid firmware upload format - no file content found");
		resetOTAState();
		return;
	}

	// Start the updater (RP2040 - try different approaches)
	REMOTE_LOG_INFO("OTA: Starting updater...");
	
	// Add diagnostic information about available space
	REMOTE_LOG_DEBUG("OTA: Flash layout - Max sketch size: 1568768 bytes");
	
	// For RP2040, try different size approaches
	bool updateStarted = false;
	
	// Try different approaches for RP2040 Update.begin()
	// With our current setup: 1.5MB sketch space, 512KB LittleFS
	// Current firmware is ~177KB, so we have plenty of space
	
	// RP2040 specific: Try with U_FLASH parameter explicitly
	// Try 1: Auto-detect with explicit flash parameter
	if (Update.begin(0, U_FLASH)) {
		updateStarted = true;
		REMOTE_LOG_DEBUG("OTA: Auto-detect with U_FLASH successful");
	} else {
		int error1 = Update.getError();
		REMOTE_LOG_DEBUG("OTA: Auto-detect U_FLASH failed with error:", String(error1).c_str());
		
		// Try 2: Default size with U_FLASH
		if (Update.begin(1024 * 1024, U_FLASH)) {
			updateStarted = true;
			REMOTE_LOG_DEBUG("OTA: 1MB with U_FLASH successful");
		} else {
			int error2 = Update.getError();
			REMOTE_LOG_DEBUG("OTA: 1MB U_FLASH failed with error:", String(error2).c_str());
			
			// Try 3: Specific small size that should definitely fit
			size_t smallSize = 512 * 1024; // 512KB
			if (Update.begin(smallSize, U_FLASH)) {
				updateStarted = true;
				REMOTE_LOG_DEBUG("OTA: 512KB with U_FLASH successful");
			} else {
				int error3 = Update.getError();
				REMOTE_LOG_DEBUG("OTA: 512KB U_FLASH failed with error:", String(error3).c_str());
				
				// Try 4: Try with exact available sketch space
				size_t maxSketchSize = 1568768 - 200000; // Leave some margin
				if (Update.begin(maxSketchSize, U_FLASH)) {
					updateStarted = true;
					REMOTE_LOG_DEBUG("OTA: Max sketch space successful");
				} else {
					int error4 = Update.getError();
					String errorMsg = "Update.begin() failed with all attempts. Errors: auto-U_FLASH(" + String(error1) + "), 1MB-U_FLASH(" + String(error2) + "), 512KB-U_FLASH(" + String(error3) + "), max-sketch(" + String(error4) + "). This may indicate a deeper RP2040 OTA configuration issue.";
					REMOTE_LOG_ERROR("OTA:", errorMsg.c_str());
					sendOTAResponse(client, 500, "Failed to start firmware update. RP2040 OTA initialization failed.");
					resetOTAState();
					return;
				}
			}
		}
	}
	
	if (!updateStarted) {
		REMOTE_LOG_ERROR("OTA: Could not start updater with any size configuration");
		sendOTAResponse(client, 500, "Failed to initialize firmware updater");
		resetOTAState();
		return;
	}

	REMOTE_LOG_INFO("OTA: Updater started successfully, receiving firmware data...");

	// Read and write firmware data - handle binary properly
	unsigned long timeout = millis() + OTA_TIMEOUT;
	uint8_t buffer[OTA_BUFFER_SIZE];
	String endBoundary = "\r\n--" + boundary;
	uint8_t boundaryBuffer[64]; // Buffer to check for boundary
	int boundaryBufferPos = 0;
	bool foundEndBoundary = false;
	
	REMOTE_LOG_DEBUG("OTA: Starting binary firmware read, looking for end boundary");
	
	while (client.connected() && millis() < timeout && !foundEndBoundary) {
		if (client.available()) {
			size_t bytesRead = client.readBytes(buffer, min(sizeof(buffer), (size_t)client.available()));
			
			if (bytesRead > 0) {
				// Check each byte for potential boundary
				for (size_t i = 0; i < bytesRead && !foundEndBoundary; i++) {
					uint8_t currentByte = buffer[i];
					
					// Add byte to boundary detection buffer
					boundaryBuffer[boundaryBufferPos] = currentByte;
					boundaryBufferPos++;
					
					// Check if we have a complete boundary
					if (boundaryBufferPos >= endBoundary.length()) {
						String boundaryCheck = "";
						for (int j = 0; j < endBoundary.length(); j++) {
							boundaryCheck += (char)boundaryBuffer[(boundaryBufferPos - endBoundary.length() + j) % 64];
						}
						
						if (boundaryCheck.equals(endBoundary)) {
							// Found end boundary - don't write the boundary bytes
							size_t writeBytes = i - endBoundary.length() + 1;
							if (writeBytes > 0) {
								size_t written = Update.write(buffer, writeBytes);
								if (written != writeBytes) {
									Update.end();
									sendOTAResponse(client, 500, "Flash write error at boundary");
									REMOTE_LOG_ERROR("OTA: Flash write failed at boundary");
									resetOTAState();
									return;
								}
								otaBytesReceived += written;
							}
							foundEndBoundary = true;
							REMOTE_LOG_DEBUG("OTA: Found end boundary, firmware complete");
							break;
						}
					}
					
					// Keep boundary buffer circular
					if (boundaryBufferPos >= 64) {
						boundaryBufferPos = 0;
					}
				}
				
				// If no boundary found in this chunk, write all the data
				if (!foundEndBoundary) {
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
					
					if (otaBytesReceived % 10240 == 0) { // Log every 10KB
						REMOTE_LOG_DEBUG("OTA: Written bytes:", otaBytesReceived);
					}
				}
			}
		} else {
			delay(10);
		}
		
		// Simple heuristic for completion without boundary
		if (!client.available() && otaBytesReceived > 50000) {
			delay(100); // Give more time for larger files
			if (!client.available()) {
				REMOTE_LOG_DEBUG("OTA: No more data available, assuming upload complete");
				break;
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

	REMOTE_LOG_INFO("OTA: Finalizing update, total bytes received:", otaBytesReceived);
	
	// Check minimum file size (typical RP2040 firmware is at least 50KB)
	if (otaBytesReceived < 50000) {
		Update.end();
		sendOTAResponse(client, 400, "Firmware file too small - likely not a valid RP2040 binary");
		REMOTE_LOG_ERROR("OTA: File too small (minimum ~50KB expected):", otaBytesReceived);
		resetOTAState();
		return;
	}
	
	// Finalize the update
	if (!Update.end(true)) {
		String error = "Update.end() failed. Error: " + String(Update.getError());
		REMOTE_LOG_ERROR("OTA:", error.c_str());
		sendOTAResponse(client, 500, "Failed to finalize firmware update: " + error);
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
