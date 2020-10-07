#pragma once

#include <stdio.h>

#if 0
#define LOG_DEBUG(x, ...) printf(x, ##__VA_ARGS__)
#define LOG_INFO(x, ...) printf(x, ##__VA_ARGS__)
#define LOG_WARNING(x, ...) printf(x, ##__VA_ARGS__)
#define LOG_ERROR(x, ...) printf(x, ##__VA_ARGS__)
#else
#define LOG_DEBUG(x, ...) do {} while(0)
#define LOG_INFO(x, ...) do {} while(0)
#define LOG_WARNING(x, ...) do {} while(0)
#define LOG_ERROR(x, ...) do {} while(0)
#endif
