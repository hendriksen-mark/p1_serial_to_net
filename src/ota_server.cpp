#include "ota_server.h"
#include "custom_log.h"
#include <base64.h>
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/bootrom.h>

// Flash memory constants
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES / 2)  // Use second half of flash

// Global variables
EthernetServer otaServer(OTA_SERVER_PORT);
OTAState otaState = OTA_IDLE;
unsigned long otaStartTime = 0;
unsigned long otaBytesReceived = 0;
unsigned long otaTotalBytes = 0;

// Statistics
unsigned long otaLastUpdate = 0;
unsigned long totalOTAAttempts = 0;
unsigned long successfulOTAUpdates = 0;
uint8_t flashBuffer[FLASH_SECTOR_SIZE];
uint32_t flashBufferPos = 0;
uint32_t flashWriteAddress = 0;

// Forward declarations
bool writeFlashSector(uint32_t offset, const uint8_t* data);
bool addToFlashBuffer(uint8_t byte);
bool flushFlashBuffer();
unsigned long lastOTAUpdate = 0;

// Temporary storage for firmware
uint8_t otaBuffer[OTA_BUFFER_SIZE];


void initializeOTAServer() {
    if (OTA_ENABLED) {
        otaServer.begin();
        REMOTE_LOG_INFO("OTA server listening on port:", OTA_SERVER_PORT);
        REMOTE_LOG_WARN("OTA enabled - Change default credentials in config.h!");
    }
}

void handleOTAConnections() {
    if (!OTA_ENABLED) return;
    
    EthernetClient client = otaServer.accept();
    if (client) {
        REMOTE_LOG_DEBUG("OTA client connected");
        totalOTAAttempts++;
        
        // Set timeout for OTA operations
        client.setTimeout(5000);
        
        // Read HTTP request
        String request = "";
        unsigned long startTime = millis();
        
        while (client.connected() && (millis() - startTime) < 10000) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                line.trim();
                
                if (line.length() == 0) {
                    // Empty line indicates end of headers
                    break;
                }
                
                if (request.length() == 0) {
                    request = line; // First line is the request
                }
                
                // Check for Authorization header
                if (line.startsWith("Authorization: ")) {
                    if (!verifyOTACredentials(line)) {
                        sendOTAResponse(client, 401, "Unauthorized");
                        client.stop();
                        REMOTE_LOG_WARN("OTA: Unauthorized access attempt");
                        return;
                    }
                }
            }
        }
        
        // Parse request
        if (request.startsWith("GET / ") || request.startsWith("GET /upload ")) {
            sendUploadPage(client);
        } else if (request.startsWith("POST /upload ")) {
            handleOTAUpload(client);
        } else if (request.startsWith("GET /status ")) {
            String status = getOTAStatus();
            sendOTAResponse(client, 200, status);
        } else {
            sendOTAResponse(client, 404, "Not Found");
        }
        
        client.stop();
    }
}

bool verifyOTACredentials(const String& authHeader) {
    // Extract base64 encoded credentials
    int spaceIndex = authHeader.indexOf(' ');
    if (spaceIndex == -1) return false;
    
    String credentials = authHeader.substring(spaceIndex + 1);
    
    // Create expected credentials
    String expected = String(OTA_USERNAME) + ":" + String(OTA_PASSWORD);
    String expectedEncoded = base64::encode(expected);
    
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
    
    // Read the multipart form data
    // This is a simplified implementation - in production, you'd want proper multipart parsing
    
    // Skip headers until we find the file data
    String line;
    bool foundBoundary = false;
    
    while (client.connected() && client.available()) {
        line = client.readStringUntil('\n');
        if (line.indexOf("Content-Type: application") >= 0) {
            foundBoundary = true;
            client.readStringUntil('\n'); // Skip empty line
            break;
        }
    }
    
    if (!foundBoundary) {
        sendOTAResponse(client, 400, "Invalid upload format");
        resetOTAState();
        return;
    }
    
    // Read firmware data
    unsigned long timeout = millis() + OTA_TIMEOUT;
    
    while (client.connected() && client.available() && millis() < timeout) {
        size_t bytesRead = client.readBytes(otaBuffer, OTA_BUFFER_SIZE);
        
        if (bytesRead > 0) {
            otaBytesReceived += bytesRead;
            
            // Check size limit
            if (otaBytesReceived > OTA_MAX_FILE_SIZE) {
                sendOTAResponse(client, 413, "Firmware file too large");
                REMOTE_LOG_ERROR("OTA: File too large:", otaBytesReceived);
                resetOTAState();
                return;
            }
            
            // Write data to flash buffer
            for (size_t i = 0; i < bytesRead; i++) {
                if (!addToFlashBuffer(otaBuffer[i])) {
                    sendOTAResponse(client, 500, "Flash write error");
                    REMOTE_LOG_ERROR("OTA: Flash write failed");
                    resetOTAState();
                    return;
                }
            }
            
            REMOTE_LOG_DEBUG("OTA: Received bytes", otaBytesReceived);
        }
        
        if (!client.available()) {
            delay(10); // Small delay to allow more data to arrive
        }
    }
    
    if (millis() >= timeout) {
        sendOTAResponse(client, 500, "Upload timeout");
        REMOTE_LOG_ERROR("OTA: Upload timeout");
        resetOTAState();
        return;
    }
    
    // Complete flash writing
    otaState = OTA_FLASHING;
    
    if (!flushFlashBuffer()) {
        sendOTAResponse(client, 500, "Flash finalization failed");
        REMOTE_LOG_ERROR("OTA: Flash finalization failed");
        resetOTAState();
        return;
    }
    
    otaState = OTA_SUCCESS;
    successfulOTAUpdates++;
    lastOTAUpdate = millis();
    
    REMOTE_LOG_INFO("OTA: Upload completed successfully - bytes written:", otaBytesReceived);
    
    sendOTAResponse(client, 200, "Upload successful! Device will reboot in 5 seconds to apply new firmware.");
    
    // Schedule reboot to boot from new firmware
    delay(5000); // Give time for response to be sent
    REMOTE_LOG_INFO("OTA: Rebooting to apply firmware...");
    
    // Reboot to bootloader which should detect new firmware
    reset_usb_boot(0, 0);
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

bool writeFlashSector(uint32_t offset, const uint8_t* data) {
    // Disable interrupts during flash operation
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase the sector first
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    
    // Write the data
    flash_range_program(offset, data, FLASH_SECTOR_SIZE);
    
    // Re-enable interrupts
    restore_interrupts(ints);
    
    return true;
}

bool addToFlashBuffer(uint8_t byte) {
    if (flashBufferPos >= FLASH_SECTOR_SIZE) {
        // Buffer full, write to flash
        if (!writeFlashSector(FLASH_TARGET_OFFSET + flashWriteAddress, flashBuffer)) {
            return false;
        }
        flashWriteAddress += FLASH_SECTOR_SIZE;
        flashBufferPos = 0;
        memset(flashBuffer, 0xFF, FLASH_SECTOR_SIZE); // Fill with 0xFF (erased state)
    }
    
    flashBuffer[flashBufferPos++] = byte;
    return true;
}

bool flushFlashBuffer() {
    if (flashBufferPos > 0) {
        // Pad buffer with 0xFF to complete the sector
        while (flashBufferPos < FLASH_SECTOR_SIZE) {
            flashBuffer[flashBufferPos++] = 0xFF;
        }
        
        if (!writeFlashSector(FLASH_TARGET_OFFSET + flashWriteAddress, flashBuffer)) {
            return false;
        }
    }
    return true;
}

void resetOTAState() {
    otaState = OTA_IDLE;
    otaStartTime = 0;
    otaBytesReceived = 0;
    otaTotalBytes = 0;
    flashBufferPos = 0;
    flashWriteAddress = 0;
    memset(flashBuffer, 0xFF, FLASH_SECTOR_SIZE);
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