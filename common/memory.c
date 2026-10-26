#include "runtime.h"
/* コンパイラが生成する構造体コピーをフリースタンディング環境で支える。 */
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *to = destination;
    const unsigned char *from = source;
    for (size_t i = 0U; i < length; ++i) { to[i] = from[i]; }
    return destination;
}
void *memset(void *destination, int value, size_t length)
{
    unsigned char *to = destination;
    for (size_t i = 0U; i < length; ++i) { to[i] = (unsigned char)value; }
    return destination;
}
