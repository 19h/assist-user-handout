#ifndef KSTR_H
#define KSTR_H

/* Obfuscated string store. Plaintext only ever exists in a caller-owned stack
 * buffer between kstr_decode() and kstr_wipe(). */
#define KSTR_BUF 64

void kstr_decode(int id, char out[KSTR_BUF]);
void kstr_wipe(char buf[KSTR_BUF]);

#endif
