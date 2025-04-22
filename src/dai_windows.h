// windows compat
#ifndef DAI_WINDOWS_H
#define DAI_WINDOWS_H

#ifdef _WIN32
#include <stdint.h>
char * strndup(const char * src, size_t size);
/* Create a realpath replacement macro for when compiling under mingw
 * Based upon https://stackoverflow.com/questions/45124869/cross-platform-alternative-to-this-realpath-definition
 */
#define realpath(N,R) _fullpath((R),(N),PATH_MAX)
#endif

#endif //DAI_WINDOWS_H
