#include <device.h>

struct returnData {
    unsigned int TSC;
    int distance;
};



struct hcsr_data {
    u32_t echo_pin;
    u32_t trigger_pin;
};

