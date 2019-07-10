#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define ADD_CLAMP(x, y, max) {(x) += (y); if ((x) > (max)) (x) = (max); if ((x) < 0) (x) = 0;}
#define ADD_CLAMP_MINMAX(x, y, min, max) {(x) += (y); if ((x) > (max)) (x) = (max); if ((x) < (min)) (x) = (min);}
#define ABS(a,b) ((a) > (b) ? ((a)-(b)) : ((b)-(a)))

typedef enum {
    MODE_LIVE,
    MODE_PLAY,
    MODE_REC
} PlaybackMode;



typedef struct {
    char name[16];
} MenuItem;

typedef struct {
    int num_items;
    int selected_item;
    MenuItem items[10];
    char value[16];
    void (*draw)(void*, int);
} Menu;



void ui_init(void);
void ui_update(void);
void draw_menu(Menu menu);

extern int part;
extern PlaybackMode mode;
extern bool redraw;

void seq_note_on(float freq);
void seq_note_off(float freq);
