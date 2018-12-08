#include "main.h"
#include "notes.h"
#include "synth.h"
#include "board.h"
#include "ui.h"
#include <math.h>

#define CONTROLLER_1    0x0E
#define CONTROLLER_2    0x0F
#define CONTROLLER_3    0x10
#define CONTROLLER_4    0x11

void midi_process_command(void);

uint8_t command[3];
bool midi_event;

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


/*void poly_add(float freq) {

    static int st = 0;

    int idx = -1;
    synth.busy = true;
    for (int i=0; i<NUM_VOICE; i++) {
        int n = i + st;
        n %= NUM_VOICE;

        if (synth.key[n] == false) {
            idx = n;
            break;
        }
        if (synth.key[n] && synth.freq[n] == freq) {
            // Note already on
            idx = n;
            break;
        }
    }
    if (idx < 0) {
        idx = st;
        synth.key_retrigger[idx] = true;
    }
    if (idx >= 0) {
        synth.freq[idx] = freq;
        synth.key[idx] = true;
        synth.key_retrigger[idx] = true;
        st++;
        st %= NUM_VOICE;
    }
    synth.busy = false;

}

void poly_del(float freq) {

    synth.busy = true;
    for (int i=0; i<NUM_VOICE; i++) {
        if (synth.freq[i] == freq) {
            synth.key[i] = false;
            synth.key_retrigger[i] = false;
        }
    }
    synth.busy = false;

}*/

void mono_add(float freq) {
    synth.busy = true;
    synth.part[0].freq = freq;
    synth.part[0].gate = true;
    synth.part[0].trig = true;
    synth.busy = false;
}

void mono_del(float freq) {
    if (freq == synth.part[0].freq) {
        synth.busy = true;
        synth.part[0].gate = false;
        synth.part[0].trig = false;
        synth.busy = false;
    }
}

void midi_process_command(void) {

    switch(command[0]) {

        // Note on
        case 0x90:
            //printf("ON     %d\r\n", command[1]);
            if (synth.seq_play) {
                seq_note_input = note[command[1]];
            } else {
                //poly_add(note[command[1]]);
                mono_add(note[command[1]]);
            }
            // Retrigger on new note in normal mode
            // if (!synth.legato && (note[command[1]] != cfg.freq)) {
            //     synth.env_retrigger = true;
            // }
            break;

        // Note off
        case 0x80:
            //printf("   OFF %d\r\n", command[1]);
            if (synth.seq_play) {
                seq_note_input = 0.0f;
            } else {
                //poly_del(note[command[1]]);
                mono_del(note[command[1]]);
            }
            break;

        // value = (float)(command[2]) / 0x7F;
        // switch (ctrlcfg) {
        //     case CTRL_MAIN:
        //         switch (command[1]) {
        //             // Master volume
        //             case CONTROLLER_1:
        //                 synth.volume = 100 * value;
        //                 printf("volume = %d\r\n", synth.volume);
        //                 break;

        //             case CONTROLLER_2:
        //                 synth.tempo = 160 * value;
        //                 printf("tempo = %d\r\n", synth.tempo);
        //                 break;

        //             case CONTROLLER_3:
        //                 synth.detune = 1.0f + 1.5f * value;
        //                 //printf("detune = %f\r\n", synth.detune);
        //                 break;
        //         }
        //         break;
        // }

        default:
            printf("%02x %02x %02x\r\n", command[0], command[1], command[2]);
    }

    midi_event = true;

}