#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
	CTRL_OSC,
	CTRL_ENVELOPE,
	CTRL_FILTER
} ControllerConfig;
extern ControllerConfig ctrlcfg;

