#include <stdio.h>
#include "WaterLevel.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while (0)
static uint16_t samples[80];
static unsigned sample_index, adc_inits, adc_pin, adc_input, delays;
void adc_init(void) { ++adc_inits; }
void adc_gpio_init(unsigned pin) { adc_pin = pin; }
void adc_select_input(unsigned input) { adc_input = input; }
uint16_t adc_read(void) { return samples[sample_index++ % 80]; }
void sleep_ms(uint32_t ms) { if (ms == 10 || ms == 60) { ++delays; } }
static void feed(uint16_t raw) {
    sample_index = delays = 0;
    for (unsigned i = 0; i < 80; ++i) { samples[i] = raw; }
}
int main(void) {
    uint16_t level=999,raw=999;
    CHECK(water_level_read(&level)==APP_NOT_READY && level==0);
    CHECK(water_level_read(NULL)==APP_ERROR);
    CHECK(water_level_read_raw(NULL)==APP_ERROR);
    CHECK(water_level_get_last_raw(NULL)==APP_ERROR);
    CHECK(water_level_capture_calibration_point(NULL)==APP_ERROR);
    CHECK(water_level_init()==APP_OK && adc_inits==1 && adc_pin==27);
    CHECK(water_level_is_calibrated());
    CHECK(water_level_get_last_raw(&raw)==APP_NOT_READY && raw==0);
    water_level_clear_calibration();
    feed(2050);
    CHECK(water_level_read(&level)==APP_NOT_READY && level==0 && sample_index==5);
    CHECK(water_level_get_last_raw(&raw)==APP_OK && raw==2050 && adc_input==1);
    WaterLevelCalibrationPoint points[4]={{10,15,20},{1000,1050,1100},{2000,2050,2100},{3000,3050,3100}};
    CHECK(water_level_set_calibration(points)==APP_OK);
    const unsigned depths[]={0,500,750,1000};
    for(unsigned i=0;i<4;++i) {
        for(unsigned adc=points[i].minimum;adc<=points[i].maximum;++adc) {
            feed((uint16_t)adc);
            CHECK(water_level_read(&level)==APP_OK && level==depths[i]);
        }
    }
    uint16_t previous=0;
    for(unsigned adc=0;adc<=4095;++adc) {
        feed((uint16_t)adc);
        CHECK(water_level_read(&level)==APP_OK && level>=previous && level<=1000);
        CHECK(sample_index==5 && delays==4);
        previous=level;
    }
    feed(1050); samples[0]=0; samples[4]=4095;
    CHECK(water_level_read(&level)==APP_OK && level==500);
    CHECK(water_level_get_last_raw(&raw)==APP_OK && raw==1050);
    /* Real overlapping readings must be rejected, without a tie-break rule. */
    WaterLevelCalibrationPoint overlap[4]={{77,88,108},{2057,2113,2160},{2149,2168,2196},{2203,2230,2254}};
    CHECK(water_level_set_calibration(overlap)==APP_ERROR);
    feed(2113); CHECK(water_level_read(&level)==APP_NOT_READY && level==0);
    points[2].minimum=1100; points[2].center=2000;
    CHECK(water_level_set_calibration(points)==APP_ERROR);
    points[2]=(WaterLevelCalibrationPoint){2000,2050,2100};
    points[3].maximum=4096; CHECK(water_level_set_calibration(points)==APP_ERROR);
    points[3].maximum=3100; points[0].center=21;
    CHECK(water_level_set_calibration(points)==APP_ERROR);
    CHECK(water_level_set_calibration(NULL)==APP_ERROR);
    points[0].center=15;
    CHECK(water_level_set_calibration(points)==APP_OK);
    feed(1050); samples[2]=4096;
    CHECK(water_level_read(&level)==APP_ERROR && level==0);
    CHECK(water_level_get_last_raw(&raw)==APP_NOT_READY && raw==0);
    for(unsigned i=0;i<80;++i) samples[i]=(uint16_t)(100+i/5);
    sample_index=0;
    WaterLevelCalibrationPoint captured;
    CHECK(water_level_capture_calibration_point(&captured)==APP_OK);
    CHECK(captured.minimum==100 && captured.center==107 && captured.maximum==115 && sample_index==80);
    feed(4096);
    CHECK(water_level_capture_calibration_point(&captured)==APP_ERROR);
    CHECK(captured.minimum==0 && captured.center==0 && captured.maximum==0);
    feed(2222); CHECK(water_level_read_raw(&raw)==APP_OK && raw==2222 && sample_index==1);
    CHECK(water_level_init()==APP_OK);
    CHECK(water_level_is_calibrated());
    water_level_clear_calibration();
    feed(1050); CHECK(water_level_read(&level)==APP_NOT_READY && level==0);
    CHECK(water_level_set_calibration(points)==APP_OK);
    water_level_clear_calibration();
    feed(1050); CHECK(water_level_read(&level)==APP_NOT_READY && level==0);
    /* Init restores the owner-selected anchors in the production driver. */
    CHECK(water_level_init()==APP_OK);
    const uint16_t anchor_raw[]={0,41,90,1850,1925,2000,2050,2099,2100,2101,4095};
    const uint16_t anchor_level[]={0,0,0,500,625,750,875,998,1000,1000,1000};
    for(unsigned i=0;i<sizeof anchor_raw/sizeof anchor_raw[0];++i) {
        feed(anchor_raw[i]);
        CHECK(water_level_read(&level)==APP_OK && level==anchor_level[i]);
    }
    previous=0;
    for(unsigned adc=0;adc<=4095;++adc) {
        feed((uint16_t)adc);
        CHECK(water_level_read(&level)==APP_OK && level>=previous && level<=1000);
        previous=level;
    }
    feed(4096);
    CHECK(water_level_read(&level)==APP_ERROR && level==0);
    CHECK(water_level_get_last_raw(&raw)==APP_NOT_READY && raw==0);
    puts("PASS production water driver: calibration validation, fixed demo anchors, all ADC inputs, median capture, boundaries and faults.");
    return 0;
}

