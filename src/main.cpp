/**************************************************************************************************/
/* Name:                                                                                          */
/*     ArduinoSerialRubberDucky                                                                   */
/* Description:                                                                                   */
/*     Real time receive, interprete and execute Duckyscript commands comming from s Serial port. */
/* Creation Date:                                                                                 */
/*     14/03/2020                                                                                 */
/* Last modified date:                                                                            */
/*     22/03/2020                                                                                 */
/**************************************************************************************************/

/* Libraries */

#include <SoftwareSerial.h>
#include <HID-Project.h>
#include "hidkeys.h"

/**************************************************************************************************/

/* Defines */

// Serial Ports communication speed bauds
#define SERIAL_BAUDS 19200
#define SWSERIAL_BAUDS 19200

// Software Serial GPIO Tx and Rx Pins
#define P_SWSERIAL_RX 8
#define P_SWSERIAL_TX 9

// Serial Reception buffer size (Maximum length for each received line)
#define RX_BUFFER_SIZE 512

/**************************************************************************************************/

/* Functions Prototypes */

// Check for incomming Serial data, store it in provided buffer and detect end of line
int8_t serial_line_received(char* my_rx_buffer, uint16_t* rx_buffer_received_bytes, 
    const size_t rx_buffer_max_size);

// Interprete and execute a Ducky Script command
// DuckyScript Documentation: https://github.com/hak5darren/USB-Rubber-Ducky/wiki/Duckyscript
int8_t ducky_script_interpreter(char* command, const uint16_t command_length);

// Convert Ducky Script key name into corresponding USB-HID Code byte
uint8_t ducky_key_to_hid_byte(const char* key);

// Count the number of words inside a string
uint32_t cstr_count_words(const char* str_in, const size_t str_in_len);

// Safe conversion a string number into uint32_t element
int8_t safe_atoi_u32(const char* in_str, const size_t in_str_len, uint32_t* out_int, 
    bool check_null_terminated=true);

/**************************************************************************************************/

/* Data Types */

// Functions Return Codes
enum _return_codes
{
    RC_OK = 0,
    RC_BAD = -1,
    RC_INVALID_INPUT = -2,
    RC_CUSTOM_DELAY = 100
};

/**************************************************************************************************/

/* Global Objects */

// Software Serial
SoftwareSerial SWSerial(P_SWSERIAL_RX, P_SWSERIAL_TX);

// Default delay between DuckyScript commands
uint32_t default_delay = 100;

/**************************************************************************************************/

/* Setup and Loop Functions */

void setup(void)
{
    // Initialize the software serial port
    Serial.begin(SERIAL_BAUDS);
    SWSerial.begin(SWSERIAL_BAUDS);

    // Initialize Keyboard
    Serial.println("Keyboard initializing...");
    Keyboard.begin();

    Serial.println("Setup done.\n");
}

void loop(void)
{
    // Serial Rx buffer and number of received bytes in the buffer
    static char _rx_buffer[SERIAL_RX_BUFFER_SIZE] = { 0 };
    static uint16_t _rx_buffer_received_bytes = 0;

    // Check for incomming Serial data lines
    if(serial_line_received(_rx_buffer, &_rx_buffer_received_bytes, SERIAL_RX_BUFFER_SIZE) == RC_OK)
    {
        // Check, interprete and execute the received line as DuckyScript command
        if(ducky_script_interpreter(_rx_buffer, _rx_buffer_received_bytes) != RC_CUSTOM_DELAY)
            delay(default_delay);
        _rx_buffer_received_bytes = 0;
    }
}

/**************************************************************************************************/

/* Serial Line Received Detector Function */

// Check for incomming Serial data, store it in provided buffer and detect end of line
int8_t serial_line_received(char* my_rx_buffer, uint16_t* rx_buffer_received_bytes, 
    const size_t rx_buffer_max_size)
{
    uint16_t i = *rx_buffer_received_bytes;

    if(i > rx_buffer_max_size-1)
        return RC_INVALID_INPUT;

    // While there is any data incoming from the hardware serial port
    while(Serial.available())
    {
        my_rx_buffer[i] = (char)Serial.read();

        i = i + 1;
        *rx_buffer_received_bytes = i;

        if(i >= rx_buffer_max_size-1)
        {
            my_rx_buffer[rx_buffer_max_size-1] = '\0';
            return RC_OK;
        }

        if((my_rx_buffer[i-1] == '\n') || (my_rx_buffer[i-1] == '\r'))
        {
            my_rx_buffer[i-1] = '\0';
            *rx_buffer_received_bytes = *rx_buffer_received_bytes - 1;
            return RC_OK;
        }
    }

    // While there is any data incoming from the software serial port
    while(SWSerial.available())
    {
        my_rx_buffer[i] = (char)SWSerial.read();

        i = i + 1;
        *rx_buffer_received_bytes = i;

        if(i >= rx_buffer_max_size-1)
        {
            my_rx_buffer[rx_buffer_max_size-1] = '\0';
            return RC_OK;
        }

        if((my_rx_buffer[i-1] == '\n') || (my_rx_buffer[i-1] == '\r'))
        {
            my_rx_buffer[i-1] = '\0';
            *rx_buffer_received_bytes = *rx_buffer_received_bytes - 1;
            return RC_OK;
        }
    }

    return RC_BAD;
}

/**************************************************************************************************/

/* Ducky Script Functions */

// Interprete and execute a Ducky Script command
// Ducky Script Documentation at: https://github.com/hak5darren/USB-Rubber-Ducky/wiki/Duckyscript
int8_t ducky_script_interpreter(char* command, const uint16_t command_length)
{
    static char last_command[SERIAL_RX_BUFFER_SIZE] = { 0 };
    char* ptr_cmd = NULL;
    char* ptr_argv = NULL;
    uint32_t argc = 0;
    uint8_t key = 0;

    // Check number of command arguments
    argc = cstr_count_words(command, command_length);
    if(argc == 0)
        return RC_BAD;
    argc = argc - 1;

    // Point to provided command line
    ptr_cmd = &(command[0]);

    Serial.print("\nCommand received: "); Serial.println(ptr_cmd);
    Serial.print("Number of command arguments: "); Serial.println(argc);

    /**********************************/

    /* Interpretation and Execution */

    // REM: Comment line, just to be ignored
    // REM [text]
    if((strncmp(ptr_cmd, "REM", strlen("REM")) == 0) || 
        (strncmp(ptr_cmd, "//", strlen("//")) == 0))
    {
        Serial.println("Comment command detected, ignoring it.");
        return RC_OK;
    }

    // REPEAT: Repeats the last command n times
    if(strncmp(ptr_cmd, "REPEAT", strlen("REPEAT")) == 0)
    {
        Serial.println("Repeat command detected.");

        // Check if there is a second argument
        if(argc == 0)
        {
            Serial.println("No arguments detected.");
            return RC_BAD;
        }

        // Ignore if no previous command available to be repeated
        if(last_command[0] == '\0')
        {
            Serial.println("No previous commands stored.");
            return RC_BAD;
        }

        // Point to second command argument
        ptr_argv = strstr(ptr_cmd, " ");
        if(ptr_argv == NULL)
            return RC_BAD;
        if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
            return RC_BAD;
        ptr_argv = ptr_argv + 1;
        Serial.print("Argument received: "); Serial.println(ptr_argv);

        // Convert argument into integer type
        uint32_t n = 0;
        if(safe_atoi_u32(ptr_argv, strlen(ptr_argv), &n) != RC_OK)
        {
            Serial.println("Can't parse to uint32_t the second argument.");
            return RC_BAD;
        }

        // Execute previous command n times through recursive call
        for(uint8_t i = 0; i < n; i++)
            ducky_script_interpreter(last_command, strlen(last_command));

        return RC_OK;
    }

    // Store this command for following command (to be used by REPEAT command)
    snprintf(last_command, SERIAL_RX_BUFFER_SIZE, "%s", command);

    // DEFAULTDELAY: Define how long (milliseconds) to wait between each subsequent command
    // DEFAULTDELAY [n]
    if((strncmp(ptr_cmd, "DEFAULT_DELAY", strlen("DEFAULT_DELAY")) == 0) || 
        (strncmp(ptr_cmd, "DEFAULTDELAY", strlen("DEFAULTDELAY")) == 0))
    {
        Serial.println("Change default delay command detected.");

        // Check if there is a second argument
        if(argc == 0)
        {
            Serial.println("No arguments detected.");
            return RC_BAD;
        }

        // Point to second command argument
        ptr_argv = strstr(ptr_cmd, " ");
        if(ptr_argv == NULL)
            return RC_BAD;
        if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
            return RC_BAD;
        ptr_argv = ptr_argv + 1;
        Serial.print("Argument received: "); Serial.println(ptr_argv);

        // Update default delay between commands values to received one
        if(safe_atoi_u32(ptr_argv, strlen(ptr_argv), &default_delay) != RC_OK)
        {
            Serial.println("Can't parse to uint32_t the second argument.");
            return RC_BAD;
        }

        return RC_OK;
    }

    // DELAY: Creates a momentary pause (ms) in the ducky script
    // DELAY [n]
    if(strncmp(ptr_cmd, "DELAY", strlen("DELAY")) == 0)
    {
        Serial.println("Delay command detected.");

        // Check if there is a second argument
        if(argc == 0)
            return RC_BAD;

        // Point to second command argument
        ptr_argv = strstr(ptr_cmd, " ");
        if(ptr_argv == NULL)
            return RC_BAD;
        if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
            return RC_BAD;
        ptr_argv = ptr_argv + 1;
        Serial.print("Argument received: "); Serial.println(ptr_argv);

        // Convert argument into integer type
        uint32_t n = 0;
        if(safe_atoi_u32(ptr_argv, strlen(ptr_argv), &n) != RC_OK)
        {
            Serial.println("Can't parse to uint32_t the second argument.");
            return RC_BAD;
        }

        // Wait for the received time
        delay(n);

        return RC_CUSTOM_DELAY;
    }

    // STRING_DELAY: Write the text waiting n milliseconds between each character
    // STRING_DELAY n text
    if(strncmp(ptr_cmd, "STRING_DELAY", strlen("STRING_DELAY")) == 0)
    {
        uint32_t delay_value = 1;

        Serial.println("String delay command detected.");

        // Check if there is a second and third arguments
        if(argc < 2)
            return RC_BAD;

        // Point to second command argument
        ptr_argv = strstr(ptr_cmd, " ");
        if(ptr_argv == NULL)
            return RC_BAD;
        if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
            return RC_BAD;
        ptr_argv = ptr_argv + 1;
        Serial.print("Argument received: "); Serial.println(ptr_argv);

        // Check index of next argument
        uint8_t delay_char_end_i = 0;
        for(uint8_t i = 0; i < strlen(ptr_argv); i++)
        {
            if(ptr_argv[i] == ' ')
                break;
            delay_char_end_i = delay_char_end_i + 1;
        }

        // Get delay value from second argument
        if(safe_atoi_u32(ptr_argv, delay_char_end_i, &delay_value, false) != RC_OK)
        {
            Serial.println("Can't parse to uint32_t the second argument.");
            return RC_BAD;
        }

        // Point to third command argument
        ptr_argv = strstr(ptr_argv, " ");
        if(ptr_argv == NULL)
            return RC_BAD;
        if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
            return RC_BAD;
        ptr_argv = ptr_argv + 1;
        Serial.print("Argument received: "); Serial.println(ptr_argv);

        // Print each character and wait between them
        for(uint32_t i = 0; i < strlen(ptr_argv); i++)
        {
            Keyboard.print(ptr_argv[i]);
            delay(delay_value);
        }

        return RC_OK;
    }

    // STRING: Processes the text following taking special care to auto-shift
    // STRING text
    if(strncmp(ptr_cmd, "STRING", strlen("STRING")) == 0)
    {
        Serial.println("String command detected.");

        // Check if there is a second argument
        if(argc == 0)
            return RC_BAD;

        // Point to second command argument
        ptr_argv = strstr(ptr_cmd, " ");
        if(ptr_argv == NULL)
            return RC_BAD;
        if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
            return RC_BAD;
        ptr_argv = ptr_argv + 1;
        Serial.print("Argument received: "); Serial.println(ptr_argv);

        Keyboard.print(ptr_argv);

        return RC_OK;
    }

    // CTRL-ALT: Press the combination Ctrl+Alt+key
    if(strncmp(ptr_cmd, "CTRL-ALT", strlen("CTRL-ALT")) == 0)
    {
        Serial.println("CTRL_ALT command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_CONTROL_LEFT);
        Keyboard.press(MOD_ALT_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // CTRL-SHIFT: Press the combination Ctrl+Shift+key
    if(strncmp(ptr_cmd, "CTRL-SHIFT", strlen("CTRL-SHIFT")) == 0)
    {
        Serial.println("CTRL-SHIFT command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_CONTROL_LEFT);
        Keyboard.press(MOD_SHIFT_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // ALT-SHIFT: Press the combination Alt-Shift keys
    if(strncmp(ptr_cmd, "ALT-SHIFT", strlen("ALT-SHIFT")) == 0)
    {
        Serial.println("ALT-SHIFT command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_ALT_LEFT);
        Keyboard.press(MOD_SHIFT_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // ALT-TAB: Press the combination Alt+TAB
    if(strncmp(ptr_cmd, "ALT-TAB", strlen("ALT-TAB")) == 0)
    {
        Serial.println("ALT_TAB command detected.");

        // Make the Key press combination
        Keyboard.press(MOD_ALT_LEFT);
        Keyboard.press(KEY_TAB);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // Windows-Alt-Key
    if((strncmp(ptr_cmd, "COMMAND-OPTION", strlen("COMMAND-OPTION")) == 0))
    {
        Serial.println("GUI+ALT command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_GUI_LEFT);
        Keyboard.press(MOD_ALT_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // GUI: Emulates the Windows-Key, sometimes referred to as the Command or Super-key
    if((strncmp(ptr_cmd, "GUI", strlen("GUI")) == 0) || 
        (strncmp(ptr_cmd, "WINDOWS", strlen("WINDOWS")) == 0) ||
        (strncmp(ptr_cmd, "COMMAND", strlen("COMMAND")) == 0))
    {
        Serial.println("GUI command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_GUI_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // CTRL: Press the Ctrl key or make a combination with it pressed
    // Arguments: BREAK, PAUSE, F1...F12, ESCAPE, ESC, Single Char
    if((strncmp(ptr_cmd, "CONTROL", strlen("CONTROL")) == 0) || 
        (strncmp(ptr_cmd, "CTRL", strlen("CTRL")) == 0))
    {
        Serial.println("CTRL command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_CONTROL_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // ALT: Press the Alt key or make a combination with it pressed
    // Arguments: END, ESC, ESCAPE, F1...F12, Single Char, SPACE, TAB
    if(strncmp(ptr_cmd, "ALT", strlen("ALT")) == 0)
    {
        Serial.println("ALT command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_ALT_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // SHIFT: Press the Shift key or make a combination with it pressed
    // Arguments: DELETE, HOME, INSERT, PAGEUP, PAGEDOWN, WINDOWS, GUI, UPARROW, DOWNARROW, 
    // LEFTARROW, RIGHTARROW, TAB
    if(strncmp(ptr_cmd, "SHIFT", strlen("SHIFT")) == 0)
    {
        Serial.println("Shift command detected.");

        // If argument provided
        if(argc > 0)
        {
            // Point to second command argument
            ptr_argv = strstr(ptr_cmd, " ");
            if(ptr_argv == NULL)
                return RC_BAD;
            if((uint16_t)(ptr_argv - ptr_cmd) >= command_length-1)
                return RC_BAD;
            ptr_argv = ptr_argv + 1;
            Serial.print("Argument received: "); Serial.println(ptr_argv);

            // Get corresponding key
            key = ducky_key_to_hid_byte(ptr_argv);
        }

        // Make the Key press combination
        Keyboard.press(MOD_SHIFT_LEFT);
        if(key != 0)
            Keyboard.press(key);
        Keyboard.releaseAll();

        return RC_OK;
    }

    // Single key commands
    key = ducky_key_to_hid_byte(ptr_cmd);
    if(key == KEY_UNDEFINED_ERROR)
    {
        Serial.println("Unknown or unsupported command received.");
        return RC_BAD;
    }

    Serial.println("Single key command.");
    Keyboard.write(KeyboardKeycode(key));
    return RC_OK;
}

// Convert Ducky Script key name into corresponding USB-HID Code byte
uint8_t ducky_key_to_hid_byte(const char* key)
{
    if(strcmp(key, "POWER") == 0)
        return KEY_POWER;
    if(strcmp(key, "HOME") == 0)
        return KEY_HOME;
    if(strcmp(key, "INSERT") == 0)
        return KEY_INSERT;
    if(strcmp(key, "PAGEUP") == 0)
        return KEY_PAGEUP;
    if(strcmp(key, "PAGEDOWN") == 0)
        return KEY_PAGEDOWN;
    if(strcmp(key, "PRINTSCREEN") == 0)
        return KEY_PRINTSCREEN;
    if(strcmp(key, "ENTER") == 0)
        return KEY_ENTER;
    if(strcmp(key, "SPACE") == 0)
        return KEY_SPACE;
    if(strcmp(key, "TAB") == 0)
        return KEY_TAB;
    if(strcmp(key, "END") == 0)
        return KEY_END;
    if(strcmp(key, "BREAK") == 0)
        return KEY_PAUSE;
    if((strcmp(key, "LEFTARROW") == 0) || (strcmp(key, "LEFT") == 0))
        return KEY_LEFT;
    if((strcmp(key, "RIGHTARROW") == 0) || (strcmp(key, "RIGHT") == 0))
        return KEY_RIGHT;
    if((strcmp(key, "DOWNARROW") == 0) || (strcmp(key, "DOWN") == 0))
        return KEY_DOWN;
    if((strcmp(key, "UPARROW") == 0) || (strcmp(key, "UP") == 0))
        return KEY_UP;
    if((strcmp(key, "ESCAPE") == 0) || (strcmp(key, "ESC") == 0))
        return KEY_ESC;
    if((strcmp(key, "DELETE") == 0) || (strcmp(key, "DEL") == 0))
        return KEY_DELETE;
    if((strcmp(key, "MENU") == 0) || (strcmp(key, "APP") == 0))
        return KEY_MENU;
    if((strcmp(key, "NUMLOCK") == 0) || (strcmp(key, "NUM_LOCK") == 0))
        return KEY_NUM_LOCK;
    if((strcmp(key, "CAPSLOCK") == 0) || (strcmp(key, "CAPS_LOCK") == 0))
        return KEY_CAPS_LOCK;
    if((strcmp(key, "SCROLLLOCK") == 0) || (strcmp(key, "SCROLL_LOCK") == 0))
        return KEY_SCROLL_LOCK;
    if((strcmp(key, "MEDIA_PLAY_PAUSE") == 0) || 
        (strcmp(key, "PLAY") == 0) || (strcmp(key, "PAUSE") == 0))
    {
        return KEY_MEDIA_PLAY_PAUSE;
    }
    if((strcmp(key, "MEDIA_STOP") == 0) || (strcmp(key, "STOP") == 0))
        return KEY_MEDIA_STOP;
    if((strcmp(key, "MEDIA_MUTE") == 0) || (strcmp(key, "MUTE") == 0))
        return KEY_MEDIA_MUTE;
    if((strcmp(key, "MEDIA_VOLUME_INC") == 0) || (strcmp(key, "VOLUMEUP") == 0))
        return KEY_MEDIA_VOLUME_INC;
    if((strcmp(key, "MEDIA_VOLUME_DEC") == 0) || (strcmp(key, "VOLUMEDOWN") == 0))
        return KEY_MEDIA_VOLUME_DEC;
    if((strcmp(key, "a") == 0) || (strcmp(key, "A") == 0))
        return KEY_A;
    if((strcmp(key, "b") == 0) || (strcmp(key, "B") == 0))
        return KEY_B;
    if((strcmp(key, "c") == 0) || (strcmp(key, "C") == 0))
        return KEY_C;
    if((strcmp(key, "d") == 0) || (strcmp(key, "D") == 0))
        return KEY_D;
    if((strcmp(key, "e") == 0) || (strcmp(key, "E") == 0))
        return KEY_E;
    if((strcmp(key, "f") == 0) || (strcmp(key, "F") == 0))
        return KEY_F;
    if((strcmp(key, "g") == 0) || (strcmp(key, "G") == 0))
        return KEY_G;
    if((strcmp(key, "h") == 0) || (strcmp(key, "H") == 0))
        return KEY_H;
    if((strcmp(key, "i") == 0) || (strcmp(key, "I") == 0))
        return KEY_I;
    if((strcmp(key, "j") == 0) || (strcmp(key, "J") == 0))
        return KEY_J;
    if((strcmp(key, "k") == 0) || (strcmp(key, "K") == 0))
        return KEY_K;
    if((strcmp(key, "l") == 0) || (strcmp(key, "L") == 0))
        return KEY_L;
    if((strcmp(key, "m") == 0) || (strcmp(key, "M") == 0))
        return KEY_M;
    if((strcmp(key, "n") == 0) || (strcmp(key, "N") == 0))
        return KEY_N;
    if((strcmp(key, "o") == 0) || (strcmp(key, "O") == 0))
        return KEY_O;
    if((strcmp(key, "p") == 0) || (strcmp(key, "P") == 0))
        return KEY_P;
    if((strcmp(key, "q") == 0) || (strcmp(key, "Q") == 0))
        return KEY_Q;
    if((strcmp(key, "r") == 0) || (strcmp(key, "R") == 0))
        return KEY_R;
    if((strcmp(key, "s") == 0) || (strcmp(key, "S") == 0))
        return KEY_S;
    if((strcmp(key, "t") == 0) || (strcmp(key, "T") == 0))
        return KEY_T;
    if((strcmp(key, "u") == 0) || (strcmp(key, "U") == 0))
        return KEY_U;
    if((strcmp(key, "v") == 0) || (strcmp(key, "V") == 0))
        return KEY_V;
    if((strcmp(key, "w") == 0) || (strcmp(key, "W") == 0))
        return KEY_W;
    if((strcmp(key, "x") == 0) || (strcmp(key, "X") == 0))
        return KEY_X;
    if((strcmp(key, "y") == 0) || (strcmp(key, "Y") == 0))
        return KEY_Y;
    if((strcmp(key, "z") == 0) || (strcmp(key, "Z") == 0))
        return KEY_Z;
    if(strcmp(key, "0") == 0)
        return KEY_0;
    if(strcmp(key, "1") == 0)
        return KEY_1;
    if(strcmp(key, "2") == 0)
        return KEY_2;
    if(strcmp(key, "3") == 0)
        return KEY_3;
    if(strcmp(key, "4") == 0)
        return KEY_4;
    if(strcmp(key, "5") == 0)
        return KEY_5;
    if(strcmp(key, "6") == 0)
        return KEY_6;
    if(strcmp(key, "7") == 0)
        return KEY_7;
    if(strcmp(key, "8") == 0)
        return KEY_8;
    if(strcmp(key, "9") == 0)
        return KEY_9;
    if(strcmp(key, "F1") == 0)
        return KEY_F1;
    if(strcmp(key, "F2") == 0)
        return KEY_F2;
    if(strcmp(key, "F3") == 0)
        return KEY_F3;
    if(strcmp(key, "F4") == 0)
        return KEY_F4;
    if(strcmp(key, "F5") == 0)
        return KEY_F5;
    if(strcmp(key, "F6") == 0)
        return KEY_F6;
    if(strcmp(key, "F7") == 0)
        return KEY_F7;
    if(strcmp(key, "F8") == 0)
        return KEY_F8;
    if(strcmp(key, "F9") == 0)
        return KEY_F9;

    return KEY_UNDEFINED_ERROR;
}

/**************************************************************************************************/

/* Auxiliar Functions */

// Count the number of words inside a string
uint32_t cstr_count_words(const char* str_in, const size_t str_in_len)
{
    uint32_t n = 1;

    // Check if string is empty
    if(str_in[0] == '\0')
        return 0;

    // Check for character occurrences
    for(size_t i = 1; i < str_in_len; i++)
    {
        // Check if end of string detected
        if(str_in[i] == '\0')
            break;

        // Check if pattern "X Y", "X\rY" or "X\nY" does not meet
        if((str_in[i] != ' ') && (str_in[i] != '\r') && (str_in[i] != '\n'))
            continue;
        if((str_in[i-1] == ' ') || (str_in[i-1] == '\r') || (str_in[i-1] == '\n'))
            continue;
        if((str_in[i+1] == ' ') || (str_in[i+1] == '\r') || (str_in[i+1] == '\n'))
            continue;
        if(str_in[i+1] == '\0')
            continue;

        // Pattern detected, increase word count
        n = n + 1;
    }

    return n;
}

// Safe conversion a string number into uint32_t element
int8_t safe_atoi_u32(const char* in_str, const size_t in_str_len, uint32_t* out_int, 
    bool check_null_terminated)
{
	size_t converted_num;
	size_t multiplicator;

	// Check if input str has less or more chars than expected int32_t range (1 to 3 chars)
	if((in_str_len < 1) || (in_str_len > 10))
		return RC_INVALID_INPUT;

	// Check if input str is not terminated
    if(check_null_terminated)
    {
	    if(in_str[in_str_len] != '\0')
		    return RC_INVALID_INPUT;
    }

	// Check if any of the character of the str is not a number
	for(uint8_t i = 0; i < in_str_len; i++)
	{
		if(in_str[i] < '0' || in_str[i] > '9')
			return RC_BAD;
	}

	// Create the int
	converted_num = 0;
	for(uint8_t i = 0; i < in_str_len; i++)
	{
		multiplicator = 1;
		for(uint8_t ii = in_str_len-1-i; ii > 0; ii--)
			multiplicator = multiplicator * 10;

		converted_num = converted_num + (multiplicator * (in_str[i] - '0'));
	}

	// Check if number is higher than max uint32_t val
	if(converted_num > UINT32_MAX)
		return RC_BAD;

	// Get the converted number and return operation success
	*out_int = (uint32_t)converted_num;
	return RC_OK;
}
