#ifndef __UTF8_H__
#define __UTF8_H__

#include <stdint.h>
#include <stdbool.h>

// Encode the given codepoint in UTF8 and write it to the provided buffer.
// Returns the number of written bytes (i.e. the UTF8 size of the character).
int LrsUTF8Encode(uint32_t cp, char out[4]);
// Given the first byte of a UTF8 character, returns the number of bytes that the character spans
int LrsUTF8CharSize(char character);
// Returns the size of the UTF8 character before the specified offset
int LrsUTF8PrevCharSize(char* str, int offset);
// Checks for a continuation byte with the 10xxxxxx pattern
bool LrsUTF8IsContinuationByte(char c);
// Counts the number of UTF8 characters included in the provided bytes buffer
int LrsUTF8CharCount(char* bytes, int size);
// Given a UTF8-encoded string, returns its length (number of characters).
int LrsUTF8StrLen(char* str);

#endif