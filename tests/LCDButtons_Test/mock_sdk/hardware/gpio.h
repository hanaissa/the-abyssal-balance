#pragma once
#include <stdbool.h>
typedef unsigned int uint;
#define GPIO_IN false
void gpio_init(uint pin);
void gpio_set_dir(uint pin, bool output);
void gpio_pull_up(uint pin);
bool gpio_get(uint pin);
