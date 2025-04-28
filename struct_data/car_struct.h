#pragma once
#include <cstring>

#pragma pack(push, 1)
struct CarDetails {
    char wi_fi[30] = {0};
    char password[30] = {0};
    char vin[18] = {0};
    char license_plate[10] = {0};
    char brand[20] = {0};
    char model[20] = {0};
    int year = 0;
    char transmission = ' ';
    char body_type[20] = {0};
    char body_number[10] = {0};
    float engine_volume = 0;
    int engine_power = 0;
    char engine_type[4] = {0};
    char color[20] = {0};
};
#pragma pack(pop)