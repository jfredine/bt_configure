#include <Arduino.h>
#include <SoftwareSerial.h>
#include <stdlib.h>

//                             RX  TX
SoftwareSerial software_serial(12, 13);

static bool try_read_line(Stream &stream,
                          char *buffer, unsigned buffer_size, char **end);

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
    static termination_requirement_t termination_requirement =
        TERMINATION_UNCONNECTED;

    // get input which may take multiple loops or have buffer overflow errors
    char *end;
    char *old_end = &buffer[strlen(buffer)];
    bool complete = try_read_line(Serial, buffer, BUFFER_SIZE, &end);
    if (end != old_end) {
        Serial.write(old_end);
    }
    if (complete) {
        Serial.println("");
    } else {
        if ((end - buffer) == (BUFFER_SIZE - 1)) {
            // overflow
            Serial.println("");
            Serial.println("Error: Buffer overflow.  Ignoring data");
            buffer[0] = '\0';
            return;
        } else {
            // not enough characters
            delay(100);
            return;
       }
    }

    // complete line of input to process
    if ((end - buffer >= 2) && (buffer[0] == 'A') && (buffer[1] == 'T')) {
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
            } else if (strcmp(args[0], "1200") &&
                       strcmp(args[0], "2400") &&
                       strcmp(args[0], "4800") &&
                       strcmp(args[0], "9600") &&
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

    buffer[0] = '\0';
    Serial.write("> ");
}


//
// bt_command
// Arguments: bt - Stream object (likely a SoftwareSerial object) to which the
//                 command will be written and from which results will be read
//            cmd - null terminated string with the command.  This buffer
//                  should NOT be terminated with \r or \n.
//            termination_requirement - determines the way the command will be
//                                      terminated to the BT device.
//            response - Character array provided by caller where response will
//                       be written.  May be NULL if response is unneeded.
//
// Returns: Number of characters written to response string if response is
//          non-NULL.  Otherwise the number of characters read from the
//          bluetooth module.  These may be different due to the way line ending
//          is handled when writing to the response string (see below).
//
// This routine will send a command to the bluetooth module serial interface
// and retreive the response.  The response will be written to the character
// array provided by the response argument.  This response will be null
// terminated and any end of line sequence (\r, \n,. or \r\n) will be converted
// to a \r\n sequence.  Additionally the response will end with \r\n even if
// not provided in the response from the bluetooth module if space exists.
//
// No more than reponse_len - 1 characters of the response will be written to
// the response array to avoid overflow while still allowing a null termination
// of the string.
//
// The same array may be used for both cmd and response if desired but this
// will overwrite the original cmd string.
//

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


//
// check_termination_requirement
//
// Arguments: Serial port object to check
//
// Returns: TERMINATION_UNCONNECTED - There is no active serial connection
//          TERMINATION_REQUIRED - A \r\n sequence is required at the end of the
//                                 command
//          TERMINATION_NOT_REQIRED - No \r\n sequence is required at the end of
//                                    the command
//          TERMINATION_UNKNOWN - Something unexpected happened and it could
//                                not be determined how the command should be
//                                terminated.
//
// This routine will attempt to determine if a \r\n sequence is required after
// AT commands to the BT module.
//

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


//
// try_read_line
//
// Arguments: stream - input stream to read (e.g. Serial)
//            buffer - character buffer for collecting input
//            buffer_size - max number of characters which may be written
//                          to the buffer
//            end - pointer to last character written ('\0') in buffer            
// Returns: true - a complete line was read
//          falsle - a complete line was not read
//
// This routine attempts to read a line of input from the provided stream.
// A line is assumed to be completed by a '\r' or '\n' character.  Detection of
// one of these characters as the first character in a line is ignored.  This
// allows for lines terminated by \n, \r, \r\n to all be recognized.  It also
// means that empty lines are ignored.  A line may not be fully recognized if:
//
//     1) No more characters are available from the input stream and no
//        end of line character was found.
//
//     2) The max buffer size was reached before an end of line character
//        was found.
//
// The user can check the length of the buffer if false is returned
// to detect condition #2.  Otherwise false means there were not enough
// characters in the input stream to complete a line.
//
// The routine appends results to the buffer with each call so the first call
// should be made with an empty buffer (buffer[0] = '\0') and subsequent calls
// can be made with partial lines until true is returned or the buffer is full.
//
// At exit from this routine the buffer will be null terminated and the end
// pointer (if not NULL) will be set to the null character.  This can be used
// to check the size of the buffer.
//

bool try_read_line(Stream &stream,
                   char *buffer, unsigned buffer_size, char **end) {
    if ((buffer == NULL) || (buffer_size == 0)) {
        return false;
    }

    unsigned buffer_index = strlen(buffer);
    while (buffer_index < (buffer_size - 1)) {
        if (stream.available()) {
            char c = stream.read();
            if ((c == '\n') || (c == '\r')) {
                if (buffer_index == 0) {
                    continue;
                } else {
                    buffer[buffer_index] = '\0';
                    if (end) {
                        *end = &buffer[buffer_index];
                    }
                    return true;
                }
            } else {
                buffer[buffer_index] = c;
                buffer_index++;
            }
        } else {
            break;
        }
    }

    buffer[buffer_index] = '\0';
    if (end) {
        *end = &buffer[buffer_index];
    }
    return false;
}
