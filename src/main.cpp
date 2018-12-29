#include <Arduino.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>
//                             RX  TX
SoftwareSerial software_serial(12, 13);

void setup() {
    Serial.begin(9600);
    software_serial.begin(9600);
    Serial.write("> ");
}

#define BUFFER_SIZE 200
void loop() {
    static char buffer[BUFFER_SIZE];
    static int buffer_idx;
    static int waiting_for_response;
    static int empty_response_count;
    static char last_data;
    char data;

    if (!waiting_for_response) {
        // accumulate data from the host
        while (Serial.available()) {
            data = Serial.read();
            // handle \r, \r\n, or \n for end of line and always echo
            // \r\n
            if ((data == '\r') || ((data == '\n') && (last_data != '\r'))) {
                buffer[buffer_idx] = '\r';
                buffer_idx++;
                buffer[buffer_idx] = '\n';
                buffer_idx++;
                Serial.write("\r\n");
                last_data = data;
                break;
            }
            else if ((data == '\n') && (last_data == '\r')) {
                last_data = data;
            }
            else {
                buffer[buffer_idx] = data;
                buffer_idx++;
                Serial.write(data);
                last_data = data;
            }
        }

        // once we get a complete line, forward to the BT module
        if ((buffer_idx > 1) && ((buffer[buffer_idx - 2] == '\r') &&
                                 (buffer[buffer_idx - 1] == '\n'))) {
            buffer[buffer_idx - 2] = '\0';
            buffer_idx -= 2;
            if (buffer_idx > 0) {
                //buffer[buffer_idx] = '\r';
                //buffer[buffer_idx + 1] = '\n';
                software_serial.write(buffer);
                buffer_idx = 0;
                waiting_for_response = 1;
                empty_response_count = 0;
                delay(1000);
            }
        }
    }
    else {
        // look for response from BT module
        if (software_serial.available()) {
            while (software_serial.available()) {
                data = software_serial.read();
                Serial.write(data);
            }
        }
        else {
            empty_response_count++;
        }
        if (empty_response_count == 2) {
            waiting_for_response = 0;
            empty_response_count = 0;
            Serial.write("\r\n> ");
        }
        delay(100);
    }
}