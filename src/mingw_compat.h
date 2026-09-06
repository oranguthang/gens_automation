/* Compatibility shim for building Gens with MinGW-w64 instead of MSVC.
 *
 * Force-included by Makefile.mingw (-include), so it is the first thing every
 * translation unit sees. It exists to make GCC behave the way the code was
 * written for, not to change what the code does.
 */

#ifndef GENS_MINGW_COMPAT_H
#define GENS_MINGW_COMPAT_H

#ifdef __GNUC__

/* MSVC's <windows.h> defines min and max as macros in C++ as well as C, and
 * Gens uses them ~500 times without qualification. MinGW deliberately omits
 * them in C++ because they collide with std::min and std::max.
 *
 * The C++ standard headers are pulled in first so that everything using
 * std::min or std::max internally -- basic_string.tcc most notably -- is
 * already parsed before the macros exist. Every standard header Gens uses is
 * listed, because a later #include of one not listed here would be parsed with
 * the macros already in force and break. Nothing in Gens calls std::min or
 * std::max by name, so the macros only ever affect Gens' own unqualified
 * calls, which is exactly what MSVC does. */
#ifdef __cplusplus
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <ios>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* MSVC's strong inline hint. */
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

#endif /* __GNUC__ */

#endif /* GENS_MINGW_COMPAT_H */
