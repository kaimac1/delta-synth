#include "main.h"
#include "notes.h"

void midi_process_command(void);

uint8_t command[3];

// Called from the MIDI UART interrupt handler
void midi_process_byte(uint8_t byte) {

    static uint8_t data_bytes = 0;

    if (byte & 0x80) {
        // Status byte
        command[0] = byte;
        data_bytes = ((byte & 0xE0) == 0xC0) ? 1 : 2;
    } else {
        // Data byte
        if (data_bytes > 0) {
            command[3 - data_bytes] = byte;
            data_bytes--;
            if (data_bytes == 0) midi_process_command();
        }
    }
    
}

void midi_process_command(void) {

    switch(command[0]) {

        // Note on
        case 0x90:
            if (command[1] == 36) { // pad 1 - toggle waveform
                cfgnew.osc_wave = (cfgnew.osc_wave == WAVE_SINE) ? WAVE_SQUARE : WAVE_SINE;
                break;
            }
            cfgnew.freq = note[command[1]];
            cfgnew.key = true;
            break;

        // Note off
        case 0x80:
            if (note[command[1]] == cfgnew.freq) {
                cfgnew.key = false;
            }
            break;

        // Controller
        case 0xB0:
            if (command[1] == 0x0E) {
                cfgnew.attack = ((float)command[2] / 0x7F) * 0.001;
                printf("attack = %f\r\n", cfgnew.attack);
            } else if (command[1] == 0x0F) {
                cfgnew.release = ((float)command[2] / 0x7F) * 0.001;
                printf("release = %f\r\n", cfgnew.release);                
            }

        default:
            printf("%02x %02x %02x\r\n", command[0], command[1], command[2]);
    }

}