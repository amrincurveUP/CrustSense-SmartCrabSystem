#include "ntc_temp.h"

#include <math.h>

#include "board_config.h"

float ntc_voltage_to_celsius(float voltage)
{
    const float vcc = 3.3f;

    if (voltage <= 0.01f || voltage >= (vcc - 0.01f)) {
        return NAN;
    }

    const float r = NTC_SERIES_OHMS * voltage / (vcc - voltage);
    const float inv_t = (1.0f / NTC_T0_KELVIN) +
                        (1.0f / NTC_B_COEFFICIENT) * logf(r / NTC_R0_OHMS);
    return (1.0f / inv_t) - 273.15f;
}
