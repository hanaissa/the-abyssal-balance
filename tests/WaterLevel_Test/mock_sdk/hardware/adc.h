#ifndef WATER_TEST_ADC_H
#define WATER_TEST_ADC_H
#include <stdint.h>
void adc_init(void);
void adc_gpio_init(unsigned pin);
void adc_select_input(unsigned input);
uint16_t adc_read(void);
#endif
