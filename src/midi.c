#include "main.h"
#include "notes.h"
#include "synth.h"
#include <math.h>

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

void arp_add(uint32_t freq) {

    cfgnew.busy = true;
    for (int i=0; i<MAX_ARP; i++) {
        if (cfgnew.arp_freqs[i] > freq) {
            for (int j=MAX_ARP-2; j>=i; j--) {
                cfgnew.arp_freqs[j+1] = cfgnew.arp_freqs[j];
            }
            cfgnew.arp_freqs[i] = freq;
            break;
        }
        if (cfgnew.arp_freqs[i] == 0) {
            cfgnew.arp_freqs[i] = freq;
            break;
        }
    }
    cfgnew.busy = false;

}

void arp_del(uint32_t freq) {

    cfgnew.busy = true;
    for (int i=0; i<MAX_ARP; i++) {
        if (cfgnew.arp_freqs[i] == freq) {
            for (int j=i; j<MAX_ARP-1; j++) {
                cfgnew.arp_freqs[j] = cfgnew.arp_freqs[j+1];
            }
            cfgnew.arp_freqs[MAX_ARP-1] = 0;
            break;
        }
    }
    cfgnew.busy = false;

}

void midi_process_command(void) {

    switch(command[0]) {

        // Note on
        case 0x90:
            if (cfgnew.arp != ARP_OFF) {
                arp_add(note[command[1]]);
                break;
            }
            // Retrigger on new note in normal mode
            if (!cfgnew.legato && (note[command[1]] != cfg.freq)) {
                cfgnew.env_retrigger = true;
            }
            cfgnew.freq = note[command[1]];
            cfgnew.key = true;
            break;

        // Note off
        case 0x80:
            if (cfgnew.arp != ARP_OFF) {
                arp_del(note[command[1]]);
                break;
            }
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

                        case CONTROLLER_2:
                            cfgnew.tempo = 160 * (float)(command[2]) / 0x7F;
                            printf("tempo = %d\r\n", cfgnew.tempo);
                            break;

                        case CONTROLLER_3:
                            cfgnew.detune = 1.0f + 1.5f * (float)(command[2]) / 0x7F;
                            printf("detune = %f\r\n", cfgnew.detune);
                            break;
                    }
                    break;


                case CTRL_ENVELOPE:
                    switch (command[1]) {
                        // Attack
                        case CONTROLLER_1:
                            cfgnew.attack = (command[2] + 1) * 0.005;
                            //cfg.attack  = 1.0 - exp(env_curve / (cfg.attack * SAMPLE_RATE));
                            //printf("attack = %f\r\n", cfgnew.attack);
                            break;

                        // Decay
                        case CONTROLLER_2:
                            cfgnew.decay   = 1.0 - exp(cfgnew.env_curve / ((command[2] + 1) * 0.005f * SAMPLE_RATE));
                            //printf("decay = %f\r\n", cfgnew.decay);
                            break;

                        // Sustain
                        case CONTROLLER_3:
                            cfgnew.sustain = (float)(command[2]) / 0x7F;
                            //printf("sustain = %f\r\n", cfgnew.sustain);
                            break;

                        // Release
                        case CONTROLLER_4:
                            cfgnew.release = 1.0 - exp(cfgnew.env_curve / ((command[2] + 1) * 0.005f * SAMPLE_RATE));
                            //printf("release = %f\r\n", cfgnew.release);
                            break;
                    }
                    break;

                case CTRL_FILTER:
                    switch (command[1]) {
                        // Cutoff
                        case CONTROLLER_1:
                            cfgnew.cutoff = 10000.0f * (float)(command[2]) / 0x7F;
                            break;

                        // Resonance
                        case CONTROLLER_2:
                            cfgnew.resonance = 3.99f * (float)(command[2]) / 0x7F;
                            break;

                        // Env mod
                        case CONTROLLER_3:
                            cfgnew.env_mod = 5000.0f * (float)(command[2]) / 0x7F;
                            break;
                    }
                    break;                    

            }
            break;

        default:
            printf("%02x %02x %02x\r\n", command[0], command[1], command[2]);
    }

}