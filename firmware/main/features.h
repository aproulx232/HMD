#include "freertos/timers.h"

void getBPM_task(void *pvParameter);
void BPMTimerCallback( TimerHandle_t xTimer );

void bleAdvt_task(void *pvParameters);

void blink_task(void *pvParameter);

void MPU_task(void *pvParameter);