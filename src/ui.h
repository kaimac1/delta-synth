#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


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

extern float seq_note_input;

extern bool seq_record;
void seq_note_on(float freq);
void seq_note_off(float freq);