#include <stdint.h>

typedef struct Dai Dai;

Dai*
dai_new();

void
dai_free(Dai* dai);

/**
 * @brief load dai script from file. If failed, abort.
 *        It will load the file and execute it. Only can be called once.
 */
void
dai_load_file(Dai* dai, const char* filename);

/**
 * @brief get global variable int value. If not found or not int, abort.
 */
int64_t
dai_get_int(Dai* dai, const char* name);

/**
 * @brief set global variable int value. If not found, abort.
 */
void
dai_set_int(Dai* dai, const char* name, int64_t value);

/**
 * @brief get global variable float value. If not found or not float, abort.
 */
double
dai_get_float(Dai* dai, const char* name);

/**
 * @brief set global variable float value. If not found, abort.
 */
void
dai_set_float(Dai* dai, const char* name, double value);

/**
 * @brief get global variable string value. If not found or not string, abort.
 *        not change the return value.
 */
const char*
dai_get_string(Dai* dai, const char* name);

/**
 * @brief set global variable string value. It will copy the string. If not found, abort.
 */
void
dai_set_string(Dai* dai, const char* name, const char* value);
