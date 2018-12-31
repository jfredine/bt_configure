#include <Arduino.h>
#include <SoftwareSerial.h>

//                             RX  TX
SoftwareSerial software_serial(12, 13);

typedef enum termination_requirement_e {
    TERMINATION_UNKNOWN,
    TERMINATION_UNCONNECTED,
    TERMINATION_REQUIRED,
    TERMINATION_NOT_REQUIRED
} termination_requirement_t;
termination_requirement_t check_termination_requirement(SoftwareSerial &port);

void setup() {
    Serial.begin(9600);
    Serial.write("> ");
}

#define MAX_CMD_ARGS 1
#define BUFFER_SIZE 200
void loop() {
    static char buffer[BUFFER_SIZE];
    static int buffer_idx;
    static bool buffer_overflow;
    static char last_data;
    static termination_requirement_t termination_requirement = TERMINATION_UNCONNECTED;

    char data;
    int empty_response_count;

    // accumulate data from the host
    while (Serial.available()) {
        data = Serial.read();
        // Throughout input processing, handle \r, \r\n, or \n as end of
        // line but always echo \r\n

        if (buffer_overflow) {
            if ((data == '\r') || (data == '\n')) {
                buffer_idx = 0;
                buffer_overflow = false;
            }
        } else if ((data != '\r') && (data != '\n') &&
                   (buffer_idx >= (BUFFER_SIZE - 2))) {
            buffer_overflow = true;
            Serial.write("buffer overflow\r\n");
        } else if ((data == '\r') || ((data == '\n') && (last_data != '\r'))) {
            buffer[buffer_idx] = '\0';
            buffer_idx++;
            Serial.write("\r\n");
        } else if ((data != '\n') && (data != '\r')) {
            buffer[buffer_idx] = data;
            buffer_idx++;
            Serial.write(data);
        }
        last_data = data;
    }

    // parse line if complete
    if ((buffer_idx > 1) && (buffer[buffer_idx - 1] == '\0')) {
        if ((buffer_idx > 2) && (buffer[0] == 'A') && (buffer[1] == 'T')) {
            if (!software_serial.isListening()) {
                Serial.write("Error: No connection established yet\r\n");
            } else {
                // write command to BT module
                software_serial.write(buffer);
                if (termination_requirement == TERMINATION_REQUIRED) {
                    software_serial.write("\r\n");
                } else {
                    delay(1000);
                }

                // look for response from BT module
                char last_response_data = '\0';
                empty_response_count = 0;
                while (empty_response_count < 2) {
                    if (software_serial.available()) {
                        while (software_serial.available() &&
                               (buffer_idx < (BUFFER_SIZE - 2))) {
                            data = software_serial.read();
                            if (data == '\r') {
                                Serial.write("\r\n");
                            }
                            else if ((data == '\n') && (last_response_data != '\r')) {
                                Serial.write("\r\n");
                            }
                            else if ((data != '\n') && (data != '\r')) {
                                Serial.write(data);
                            }
                            last_response_data = data;
                        }
                        empty_response_count = 0;
                    } else {
                        empty_response_count++;
                    }
                    delay(100);
                }
                if ((last_response_data != '\r') && (last_response_data != '\n')) {
                    Serial.write("\r\n");
                }
            }
        } else {
            char *command;
            char *args[MAX_CMD_ARGS + 1];

            command = strtok(buffer, " \t\n");
            for (int i = 0; i <= MAX_CMD_ARGS; i++) {
                args[i] = strtok(NULL, " \t\n");
            }

            if (!strcmp(command, "open")) {
                if (args[0] == NULL) {
                    Serial.write("Error: open requires one argument\r\n");
                } else if (args[1] != NULL) {
                    Serial.write("Error: open requires one argument\r\n");
                } else if (strcmp(args[0], "1200") && strcmp(args[0], "2400") &&
                           strcmp(args[0], "4800") && strcmp(args[0], "9600") &&
                           strcmp(args[0], "19200") &&
                           strcmp(args[0], "38400") &&
                           strcmp(args[0], "57600") &&
                           strcmp(args[0], "115200")) {
                    Serial.write("Error: Unsupported baud rate\r\n");
                } else {
                    long baud_rate = strtol(args[0], NULL, 10);
                    if (software_serial.isListening()) {
                        software_serial.end();
                    }
                    software_serial.begin(baud_rate);
                    termination_requirement =
                        check_termination_requirement(software_serial);
                }
            } else if (!strcmp(command, "close")) {
                if (args[0] != NULL) {
                    Serial.write("Error: close requires no arguments\r\n");
                } else {
                    if (software_serial.isListening()) {
                        software_serial.end();
                        termination_requirement = TERMINATION_UNCONNECTED;
                    }
                }
            } else {
                Serial.write("Error: Unknown command\r\n");
            }
        }
        Serial.write("> ");
        buffer_idx = 0;
    }
}

/*
 * check_termination_requirement
 * Arguments: Serial port object to check
 * Returns: TERMINATION_UNCONNECTED - There is no active serial connection
 *          TERMINATION_REQUIRED - A \r\n sequence is required at the end of the command
 *          TERMINATION_NOT_REQIRED - No \r\n sequence is required at the end f the command
 *          TERMINATION_UNKNOWN - Something unexpected happened and it could
 *                                not be determined how the command should be terminated.
 * 
 * This routine will attempt to determine if a \r\n sequence is required after
 * AT commands to the BT module.
 */

termination_requirement_t check_termination_requirement(SoftwareSerial &port) {
    int empty_response_count = 0;
    char data;
    char buffer[BUFFER_SIZE];
    int  buffer_idx;

    if (!port.isListening()) {
        return TERMINATION_UNCONNECTED;
    }

    /* send command with no termination and check results */
    port.write("AT");
    delay(1000);

    buffer_idx = 0;
    empty_response_count = 0;
    while (empty_response_count < 2) {
        if (port.available()) {
            while (port.available() && (buffer_idx < (BUFFER_SIZE - 2))) {
                data = software_serial.read();
                buffer[buffer_idx] = data;
                buffer_idx++;
            }
            empty_response_count = 0;
        }
        else {
            empty_response_count++;
        }
        delay(100);
    }
    if ((buffer_idx >= 2) && (buffer[0] == 'O') && (buffer[1] == 'K')) {
        return TERMINATION_NOT_REQUIRED;
    }

    // add the line ending and check results
    port.write("\r\n");
 
    buffer_idx = 0;
    empty_response_count = 0;
    while (empty_response_count < 2) {
        if (port.available()) {
            while (port.available() && (buffer_idx < (BUFFER_SIZE - 2))) {
                data = port.read();
                buffer[buffer_idx] = data;
                buffer_idx++;
            }
            empty_response_count = 0;
        }
        else {
            empty_response_count++;
        }
        delay(100);
    }
    if ((buffer_idx >= 2) && (buffer[0] == 'O') && (buffer[1] == 'K')) {
        return TERMINATION_REQUIRED;
    }
    if (buffer_idx > 0) {
        Serial.write(buffer);
        Serial.write("maybe\r\n");
    }

    return TERMINATION_UNKNOWN;
}
