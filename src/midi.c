#include "main.h"
#include "notes.h"
#include "synth.h"

#define CONTROLLER_1    0x0E
#define CONTROLLER_2    0x0F
#define CONTROLLER_3    0x10
#define CONTROLLER_4    0x11

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
            // if (command[1] == 36) { // pad 1 - toggle waveform
            //     cfgnew.osc_wave = (cfgnew.osc_wave == WAVE_SINE) ? WAVE_SQUARE : WAVE_SINE;
            //     break;
            // }
            if (note[command[1]] != cfg.freq) {
                cfgnew.env_retrigger = true;
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
            switch (ctrlcfg) {
                case CTRL_MAIN:
                    switch (command[1]) {
                        // Master volume
                        case CONTROLLER_1:
                            cfgnew.volume = 100 * (float)(command[2]) / 0x7F;
                            printf("volume = %d\r\n", cfgnew.volume);
                            break;
                    }
                    break;


                case CTRL_ENVELOPE:
                    switch (command[1]) {
                        // Attack
                        case CONTROLLER_1:
                            cfgnew.attack = (command[2] + 1) * 0.005;
                            //printf("attack = %f\r\n", cfgnew.attack);
                            break;

                        // Decay
                        case CONTROLLER_2:
                            cfgnew.decay = (command[2] + 1) * 0.005;
                            //printf("decay = %f\r\n", cfgnew.decay);
                            break;

                        // Sustain
                        case CONTROLLER_3:
                            cfgnew.sustain = (float)(command[2]) / 0x7F;
                            //printf("sustain = %f\r\n", cfgnew.sustain);
                            break;

                        // Release
                        case CONTROLLER_4:
                            cfgnew.release = (command[2] + 1) * 0.005;
                            //printf("release = %f\r\n", cfgnew.release);
                            break;
                    }
                    break;

                case CTRL_FILTER:
                    switch (command[1]) {
                        // Cutoff
                        case CONTROLLER_1:
                            cfgnew.cutoff = 10000.0f * (float)(command[2]) / 0x7F;
                            //printf("fc = %.1f\r\n", cfgnew.cutoff);
                            break;

                        // Resonance
                        case CONTROLLER_2:
                            cfgnew.resonance = 3.99f * (float)(command[2]) / 0x7F;
                            //printf("k = %.3f\r\n", cfgnew.resonance);
                            break;

                        // Env mod
                        case CONTROLLER_3:
                            cfgnew.env_mod = 5000.0f * (float)(command[2]) / 0x7F;
                            //printf("envmod = %.1f\r\n", cfgnew.envmod);
                            break;
                    }
                    break;                    

            }
            break;

        default:
            printf("%02x %02x %02x\r\n", command[0], command[1], command[2]);
    }

}