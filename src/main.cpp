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
int bt_command(Stream &bt, const char *cmd,
               termination_requirement_t termination_requirement,
               char *response, int response_max_len);

void setup() {
    Serial.begin(9600);
    Serial.write("> ");
}

#define MAX_CMD_ARGS 1
#define BUFFER_SIZE 200
void loop() {
    static char buffer[BUFFER_SIZE];
    static int buffer_idx;
    static char last_data;
    static termination_requirement_t termination_requirement =
        TERMINATION_UNCONNECTED;

    char data;

    // accumulate data from the host
    while (Serial.available()) {
        data = Serial.read();
        // Throughout input processing, handle \r, \r\n, or \n as end of
        // line but always echo \r\n

        if (buffer_idx >= BUFFER_SIZE) {
            if (buffer_idx == BUFFER_SIZE) {
                Serial.write("buffer overflow\r\n");
            }
            if ((data == '\r') || (data == '\n')) {
                buffer_idx = 0;
            } else {
                buffer_idx++;
            }
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
                if (bt_command(software_serial, buffer, termination_requirement,
                               buffer, BUFFER_SIZE)) {
                    Serial.write(buffer);
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
 * bt_command
 * Arguments: bt - Stream object (likely a SoftwareSerial object) to which the
 *                 command will be written and from which results will be read
 *            cmd - null terminated string with the command.  This buffer
 *                  should NOT be terminated with \r or \n.
 *            termination_requirement - determines the way the command will be
 *                                      terminated to the BT device.
 *            response - Character array provided by caller where response will
 *                       be written.  May be NULL if response is unneeded.
 *
 * Returns: Number of characters written to response string if response is
 *          non-NULL.  Otherwise the number of characters read from the
 *          bluetooth module.  These may be different due to the way line ending
 *          is handled when writing to the response string (see below).
 *
 * This routine will send a command to the bluetooth module serial interface
 * and retreive the response.  The response will be written to the character
 * array provided by the response argument.  This response will be null
 * terminated and any end of line sequence (\r, \n,. or \r\n) will be converted
 * to a \r\n sequence.  Additionally the response will end with \r\n even if
 * not provided in the response from the bluetooth module if space exists.
 *
 * No more than reponse_len - 1 characters of the response will be written to
 * the response array to avoid overflow while still allowing a null termination
 * of the string.
 *
 * The same array may be used for both cmd and response if desired but this
 * will overwrite the original cmd string.
 */

int bt_command(Stream &bt, const char *cmd,
               termination_requirement_t termination_requirement,
               char *response, int response_max_len) {

    // write command to BT module
    if (cmd) {
        bt.write(cmd);
    }
    if (termination_requirement == TERMINATION_REQUIRED) {
        bt.write("\r\n");
    } else {
        delay(1000);
    }

    // look for response
    char last_response_data = '\0';
    int response_bytes = 0;
    int empty_response_count = 0;
    int response_idx = 0;
    while (empty_response_count < 2) {
        if (bt.available()) {
            while (bt.available()) {
                char data = bt.read();
                response_bytes++;
                if (response && (response_idx < (response_max_len - 1))) {
                    if ((data == '\r') ||
                        ((data == '\n') && (last_response_data != '\r'))) {
                        response[response_idx] = data;
                        response_idx++;
                        if (response_idx < (response_max_len - 1)) {
                            response[response_idx] = '\n';
                            response_idx++;
                        }
                    } else if ((data != '\n') && (data != '\r')) {
                        response[response_idx] = data;
                        response_idx++;
                    }
                    last_response_data = data;
                }
            }
            empty_response_count = 0;
        } else {
            empty_response_count++;
        }
        delay(100);
    }

    if ((last_response_data != '\r') && (last_response_data != '\n')) {
        if (response && (response_idx < (response_max_len - 1))) {
            response[response_idx] = '\r';
            response_idx++;
            if (response_idx < (response_max_len - 1)) {
                response[response_idx] = '\n';
                response_idx++;
            }
        }
    }

    if (response) {
        if (response_idx < response_max_len) {
            response[response_idx] = '\0';
        } else {
            response[response_max_len - 1] = '\0';
        }
        return strlen(response);
    }

    return response_bytes;
}

/*
 * check_termination_requirement
 * Arguments: Serial port object to check
 * Returns: TERMINATION_UNCONNECTED - There is no active serial connection
 *          TERMINATION_REQUIRED - A \r\n sequence is required at the end of the
 * command TERMINATION_NOT_REQIRED - No \r\n sequence is required at the end f
 * the command TERMINATION_UNKNOWN - Something unexpected happened and it could
 *                                not be determined how the command should be
 * terminated.
 *
 * This routine will attempt to determine if a \r\n sequence is required after
 * AT commands to the BT module.
 */

termination_requirement_t check_termination_requirement(SoftwareSerial &port) {
    if (!port.isListening()) {
        return TERMINATION_UNCONNECTED;
    }

    /* send command with no termination and check results */
    char response[3];
    bt_command(port, "AT", TERMINATION_NOT_REQUIRED, response, 3);
    if (!strcmp(response, "OK")) {
        return TERMINATION_NOT_REQUIRED;
    }

    // add the line ending and check results
    bt_command(port, NULL, TERMINATION_REQUIRED, response, 3);
    if (!strcmp(response, "OK")) {
        return TERMINATION_REQUIRED;
    }

    return TERMINATION_UNKNOWN;
}
