#ifndef INPUT_H
#define INPUT_H

#include "common.h"

void
input_update(void);

void
input_init(void);

void
input_add_hotkey(fchar* string, void *call_back);

void
input_deinit(void);

#endif /* INPUT_H */
