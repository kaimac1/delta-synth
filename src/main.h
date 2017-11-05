#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdio.h>
#include "stm32f401_discovery.h"
#include "stm32f401_discovery_audio.h"
#include "audio_play.h"
#include "board.h"

typedef enum {
	CTRL_MAIN,
	CTRL_ENVELOPE,
} ControllerConfig;
extern ControllerConfig ctrlcfg;


#endif /* __MAIN_H */
