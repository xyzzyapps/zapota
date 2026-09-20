/* config.h - Minimal LAME config for static zig cc cross-platform build */
#ifndef LAME_CONFIG_H
#define LAME_CONFIG_H

#define STDC_HEADERS 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MATH_H 1

/* Float precision for internal calculations */
typedef float ieee754_float32_t;
typedef double ieee754_float64_t;
typedef long double ieee854_float80_t;

/* Version info */
#define PACKAGE_VERSION "3.100"
#define PACKAGE_STRING  "lame 3.100"
#define PACKAGE_NAME    "lame"

/* Disable optional components */
#define HAVE_MPGLIB 0

/* Nasm disabled - must undefine so #ifdef HAVE_NASM evaluates to false */
#undef HAVE_NASM

/* Disable VBR features that need extra deps */
#define HAVE_VBRTAG 1

/* Use integer types from stdint.h */
#include <stdint.h>

#endif /* LAME_CONFIG_H */
