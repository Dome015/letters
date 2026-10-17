#include "utf8.h"

// Encode the given codepoint in UTF8 and write it to the provided buffer.
// Returns the number of written bytes (i.e. the UTF8 size of the character).
int LrsUTF8Encode(uint32_t cp, char out[4]) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        return 0;
    }
    if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

// Given the first byte of a UTF8 character, returns the number of bytes that the character spans
int LrsUTF8CharSize(char character) {
    if ((character & 0x80) == 0x00) {
        return 1; // 0xxxxxxx
    }
    if ((character & 0xE0) == 0xC0) {
        return 2; // 110xxxxx
    }
    if ((character & 0xF0) == 0xE0) {
        return 3; // 1110xxxx
    }
    if ((character & 0xF8) == 0xF0) {
        return 4; // 11110xxx
    }
    return -1;
}

// Returns the size of the UTF8 character before the specified offset
int LrsUTF8PrevCharSize(char* str, int offset) {
    int char_size = 1;
    while (LrsUTF8IsContinuationByte(str[offset - char_size])) {
        char_size += 1;
    }
    return char_size;
}

// Checks for a continuation byte with the 10xxxxxx pattern
bool LrsUTF8IsContinuationByte(char c) {
    return (c & 0xC0) == 0x80;
}

// Counts the number of UTF8 characters included in the provided bytes buffer
int LrsUTF8CharCount(char* bytes, int size) {
    int count = 0;
    for (int i = 0; i < size; i++) {
        if (!LrsUTF8IsContinuationByte(bytes[i])) {
            count += 1;
        }
    }
    return count;
}

// Given a UTF8-encoded string, returns its length (number of characters).
int LrsUTF8StrLen(char* str) {
    int pos = 0;
    int len = 0;
    while (str[pos] != '\0') {
        len += 1;
        pos += LrsUTF8CharSize(str[pos]);
    }
    return len;
}