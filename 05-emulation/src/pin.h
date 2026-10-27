#ifndef PIN_H
#define PIN_H
#include <stdint.h>

/* Daily unlock PIN for a lock serial. Pure: no libc, no globals besides the
 * read-only S-box, so it can be emulated in isolation. */
uint32_t derive_unlock_pin(const char *serial, uint32_t day);
int serial_is_valid(const char *serial);

#endif
