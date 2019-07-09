#include "main.h"
#include "board.h"
#include "notes.h"
#include "synth.h"
#include "ui.h"

void midi_process_command(void);
void note_on(float freq);
void note_off(float freq);

uint8_t command[3];
bool midi_event;


// Called from the MIDI UART interrupt handler
void midi_process_byte(uint8_t byte) {

    static uint8_t data_bytes = 0;
    static int idx = 0;

    if (byte & 0x80) {
        // Status byte
        idx = 0;
        command[idx++] = byte;
        data_bytes = ((byte & 0xE0) == 0xC0) ? 1 : 2;

    } else {
        // Data byte
        if (data_bytes > 0) {
            command[idx++] = byte;
            data_bytes--;
            if (data_bytes == 0) midi_process_command();
        }
    }
    
}

void midi_process_command(void) {

    switch (command[0]) {

        // Note on
        case 0x90:
            note_on(note[command[1]]);
            // Retrigger on new note in normal mode
            // if (!synth.legato && (note[command[1]] != cfg.freq)) {
            //     synth.env_retrigger = true;
            // }
            break;

        // Note off
        case 0x80:
            note_off(note[command[1]]);
            break;

        default:
            // printf("%02x %02x %02x\r\n", command[0], command[1], command[2]);
            break;
            
    }

    midi_event = true;

}




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

void note_on(float freq) {

    if (mode == MODE_REC) {
        seq_note_on(freq);
    } else {
        mono_add(freq);
    }

}
void note_off(float freq) {

    if (mode == MODE_REC) {
        seq_note_off(freq);
    } else {
        mono_del(freq);
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

