#pragma once

#include <stdint.h>

#ifndef __VA_H
#define __VA_H

#ifndef _WIN32
typedef uint8_t     BYTE;
typedef uint16_t    WORD;
typedef uint32_t    DWORD;
typedef uint64_t    QWORD;
#endif

#ifdef _WIN64
typedef uint64_t    va_t;
#else
typedef uint32_t    va_t;
#endif

#endif
