//
// Created by Markus on 3.5.2025.
//

#ifndef RTCMBUFFER_H
#define RTCMBUFFER_H
#include <stdint.h>


namespace rtcmbuffer
{
void init();
void process_byte(uint8_t byte, void (*forward_func)(const uint8_t *, int));
}



#endif //RTCMBUFFER_H
