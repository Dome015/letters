#include "raylib.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#define LRS_GREEN ((Color){ 55, 117, 0, 255 })

#define SPACING 0

typedef struct {
    int x;
    int y;
} IntVec2;

typedef struct {
    int start;
    int length;
    int char_start;
    int char_length;
} LineInfo;

typedef struct {
    // Settings
    Font font;
    int font_size;
    int textbox_padding;
    // State
    bool dirty;
    bool selecting;
    float timing_offset;
    char text[1024];
    int line_count;
    LineInfo lines[1024];
    Vector2 mouse_pos;
    IntVec2 mouse_char_pos;
    Vector2 char_size;
    int caret_offset;
    int byte_count;
    int char_count;
    int preferred_line_char;
    Rectangle textbox_rect;
} LrsState;

// Utilities

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

// Checks for a continuation byte with the 10xxxxxx pattern
bool LrsUTF8IsContinuationByte(char c) {
    return (c & 0xC0) == 0x80;
}

int LrsUTF8StrLen(char* str) {
    int pos = 0;
    int len = 0;
    while (str[pos] != '\0') {
        len += 1;
        pos += LrsUTF8CharSize(str[pos]);
    }
    return len;
}

// Returns the line number corresponding to the specified position, or -1 if the position is not valid
int LrsGetLineIndexFromPos(LrsState state, int offset) {
    if (offset < 0 || offset > state.byte_count) {
        return -1;
    }
    int low = 0;
    int high = state.line_count - 1;
    for (int i = 0; i < state.line_count; i++) {
        int mid = (high + low) / 2;
        LineInfo line = state.lines[mid];
        bool has_next = mid < state.line_count - 1;
        LineInfo next_line = has_next ? state.lines[mid + 1] : line;
        if (line.start == offset) {
            return mid;
        } else if (line.start > offset) {
            high = mid - 1;
        } else if (!has_next || next_line.start > offset) {
            return mid;
        } else {
            low = mid + 1;
        }
    }
    return -1;
}

// Returns the line corresponding to the specified position, a placeholder if the position is not valid
LineInfo LrsGetLineFromPos(LrsState state, int offset) {
    int line_index = LrsGetLineIndexFromPos(state, offset);
    if (line_index == -1) {
        return (LineInfo){ 0 };
    }
    return state.lines[line_index];
}

// Given a line info and a number of characters, returns the offset corresponding to the first n characters of that line
int LrsGetLineOffsetFromCharCount(char* text, LineInfo line, int char_count) {
    int counted = 0;
    int offset = 0;
    while (counted < char_count) {
        counted += 1;
        offset += LrsUTF8CharSize(text[line.start + offset]);
    }
    return offset;
}

// Given a line info and an offset, returns the number of characters corresponding to that offset on that line
int LrsGetLineCharCountFromOffset(char* text, LineInfo line, int offset) {
    int counted = 0;
    int applied_offset = 0;
    while (applied_offset < offset) {
        counted += 1;
        applied_offset += LrsUTF8CharSize(text[line.start + applied_offset]);
    }
    return counted;
}

// Given the row an col position of a character, returns the corresponding byte offset
int LrsGetOffsetFromCharPos(LrsState state, IntVec2 char_pos) {
    if (char_pos.x < 0 || char_pos.y < 0) {
        return -1;
    }
    int line_index = char_pos.y >= state.line_count ?
        state.line_count - 1 : char_pos.y;
    LineInfo line = state.lines[line_index];
    return char_pos.x >= line.char_length ?
        line.start + line.length : line.start + LrsGetLineOffsetFromCharCount(state.text, line, char_pos.x);
}

// Inserts characters at the given position, updating the state accordingly
void LrsInsertBytes(LrsState* state, int offset, char* bytes, int size) {
    if (size == 0) {
        return;
    }
    int chars_processed = 0;
    int index = 0;
    while (index < size) {
        int char_size = LrsUTF8CharSize(bytes[index]);
        chars_processed += 1;
        index += char_size;
    }
    // Shift all characters after the offset ahead
    for (int i = state->byte_count + size; i >= offset + size; i--) {
        state->text[i] = state->text[i - size];
    }
    // Insert new bytes
    for (int i = 0; i < size; i++) {
        state->text[state->caret_offset + i] = bytes[i];
    }
    state->byte_count += size;
    state->char_count += chars_processed;
    state->dirty = true;
}

// Deletes the specified number of VISIBLE characters going forward from the specified offset.
// Returns the number of deleted bytes.
int LrsDeleteCharsForward(LrsState* state, int offset, int count) {
    if (count == 0) {
        return 0;
    }
    int deleted_size = 0;
    for (int i = 0; i < count; i++) {
        deleted_size += LrsUTF8CharSize(state->text[offset + deleted_size]);
    }
    for (int i = offset; i <= state->byte_count - deleted_size ; i++) {
        state->text[i] = state->text[i + deleted_size];
    }
    state->byte_count -= deleted_size;
    state->char_count -= count;
    state->dirty = true;
    return deleted_size;
}

// Deletes the specified number of VISIBLE characters going backwards from the specified offset.
// Returns the number of deleted bytes.
int LrsDeleteCharsBack(LrsState* state, int offset, int count) {
    if (count == 0) {
        return 0;
    }
    int deleted_size = 0;
    int deleted_chars = 0;
    while (deleted_chars < count) {
        deleted_size += 1;
        if (!LrsUTF8IsContinuationByte(state->text[offset - deleted_size])) {
            deleted_chars += 1;
        }
    }
    for (int i = offset - deleted_size; i <= state->byte_count - deleted_size; i++) {
        state->text[i] = state->text[i + deleted_size];
    }
    state->byte_count -= deleted_size;
    state->char_count -= count;
    state->dirty = true;
    return deleted_size;
}

// State management

void LrsUpdateState(LrsState* state) {
    state->mouse_pos = GetMousePosition();
    state->char_size = MeasureTextEx(state->font, "A", state->font_size, SPACING);
    int textbox_w = GetScreenWidth();
    int textbox_h = GetScreenHeight() - 32;
    int padding = state->textbox_padding;
    // Current textbox bounding rect
    state->textbox_rect = (Rectangle){ padding, padding, textbox_w - padding, textbox_h - padding };
    // Compute the position of the character to which the mouse is pointing, or (-1, -1) if it's not pointing at one
    if (state->mouse_pos.x > state->textbox_rect.width || state->mouse_pos.x < state->textbox_rect.x || state->mouse_pos.y > state->textbox_rect.height || state->mouse_pos.y < state->textbox_rect.y) {
        state->mouse_char_pos = (IntVec2) { -1, -1 };
    } else {
        state->mouse_char_pos = (IntVec2) { (int) (state->mouse_pos.x - state->textbox_rect.x + state->char_size.x * 0.25) / state->char_size.x, (int) (state->mouse_pos.y - state->textbox_rect.y) / state->char_size.y };
    }
    // Update line info
    if (state->dirty) {
        state->line_count = 0;
        int length = 0;
        int char_length = 0;
        int offset = 0;
        int char_count = 0;
        char chr = state->text[0];
        int start = offset;
        int char_start = char_count;
        while (chr != '\0') {
            if (chr == '\n') {
                LineInfo line = (LineInfo) {
                    .start = start,
                    .char_start = char_start,
                    .length = length,
                    .char_length = char_length,
                };
                state->lines[state->line_count] = line;
                state->line_count += 1;
                start = offset + LrsUTF8CharSize(chr);
                char_start = char_count + 1;
                char_length = 0;
                length = 0;
            } else {
                char_length += 1;
                length += LrsUTF8CharSize(chr);
            }
            char_count += 1;
            offset += LrsUTF8CharSize(chr);
            chr = state->text[offset];
        }
        LineInfo line = (LineInfo) {
            .start = start,
            .char_start = char_start,
            .length = length,
            .char_length = char_length,
        };
        state->lines[state->line_count] = line;
        state->line_count += 1;
        state->dirty = false;
        state->char_count = char_count;
        state->byte_count = offset;
    }
    // INPUT HANDLING
    // Mouse click to move the caret to a point
    if (state->mouse_char_pos.x != -1) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int mouse_offset = LrsGetOffsetFromCharPos(*state, state->mouse_char_pos);
            state->caret_offset = mouse_offset;
            state->selecting = true;
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state->selecting = false;
    }
    // Handle pressed keys
    int pressed_key;
    while ((pressed_key = GetKeyPressed()) != 0) {
        switch (pressed_key) {
        // Arrow keys to move the caret
        case KEY_LEFT: {
            if (state->caret_offset == 0) {
                break;
            }
            int char_size = 1;
            while (LrsUTF8IsContinuationByte(state->text[state->caret_offset - char_size])) {
                char_size += 1;
            }
            state->caret_offset -= LrsUTF8CharSize(state->text[state->caret_offset - char_size]);
            LineInfo caret_line = LrsGetLineFromPos(*state, state->caret_offset);
            int caret_line_offset = state->caret_offset - caret_line.start;
            state->preferred_line_char = caret_line_offset;
        } break;
        case KEY_RIGHT: {
            if (state->caret_offset == state->lines[state->line_count - 1].start + state->lines[state->line_count - 1].length) {
                break;
            }
            state->caret_offset += LrsUTF8CharSize(state->text[state->caret_offset]);
            LineInfo caret_line = LrsGetLineFromPos(*state, state->caret_offset);
            int caret_line_offset = state->caret_offset - caret_line.start;
            state->preferred_line_char = caret_line_offset;
        } break;
        case KEY_UP: {
            int caret_line_number = LrsGetLineIndexFromPos(*state, state->caret_offset);
            LineInfo caret_line = state->lines[caret_line_number];
            int caret_line_offset = state->caret_offset - caret_line.start;
            int caret_line_char = LrsGetLineCharCountFromOffset(state->text, caret_line, caret_line_offset);
            if (caret_line_number == 0) {
                state->caret_offset = 0;
            } else {
                LineInfo prev_line = state->lines[caret_line_number - 1];
                if (prev_line.char_length >= caret_line_char) {
                    state->caret_offset = prev_line.start + LrsGetLineOffsetFromCharCount(state->text, prev_line, caret_line_char);
                } else {
                    state->preferred_line_char = caret_line_char;
                    state->caret_offset = caret_line.start - 1;
                }
            }
        } break;
        case KEY_DOWN: {
            int caret_line_number = LrsGetLineIndexFromPos(*state, state->caret_offset);
            LineInfo caret_line = state->lines[caret_line_number];
            int caret_line_offset = state->caret_offset - caret_line.start;
            int caret_line_char = LrsGetLineCharCountFromOffset(state->text, caret_line, caret_line_offset);
            if (caret_line_number == state->line_count - 1) {
                state->caret_offset = caret_line.start + caret_line.length;
            } else {
                LineInfo next_line = state->lines[caret_line_number + 1];
                if (next_line.char_length >= caret_line_char) {
                    state->caret_offset = next_line.start + LrsGetLineOffsetFromCharCount(state->text, next_line, caret_line_char);
                } else {
                    state->preferred_line_char = caret_line_char;
                    state->caret_offset = next_line.start + next_line.length;
                }
            }
        } break;
        case KEY_BACKSPACE: {
            if (state->caret_offset == 0) {
                break;
            }
            state->caret_offset -= LrsDeleteCharsBack(state, state->caret_offset, 1);
        } break;
        case KEY_DELETE: {
            if (state->caret_offset == state->byte_count) {
                break;
            }
            LrsDeleteCharsForward(state, state->caret_offset, 1);
        } break;
        case KEY_ENTER: {
            LrsInsertBytes(state, state->caret_offset, "\n", 1);
            state->caret_offset += 1;
        } break;
        default:
            break;
        }
    }
    // Type characters
    int pressed_char;
    while ((pressed_char = GetCharPressed()) != 0) {
        char to_add[4];
        int byte_count = LrsUTF8Encode(pressed_char, to_add);
        LrsInsertBytes(state, state->caret_offset, to_add, byte_count);
        state->caret_offset += byte_count;
    }
}

// Drawing

void LrsDrawTextBox(LrsState state) {
    DrawTextEx(state.font, state.text, (Vector2) { state.textbox_padding, state.textbox_padding }, state.font_size, 0, BLACK);
    // Caret
    int line_number = LrsGetLineIndexFromPos(state, state.caret_offset);
    int caret_y = state.char_size.y * line_number;
    int caret_char_offset = LrsGetLineCharCountFromOffset(state.text, state.lines[line_number], state.caret_offset - state.lines[line_number].start);
    int caret_x = state.char_size.x * caret_char_offset;
    int caret_w = state.char_size.x * 0.4;
    bool blink = sin(8 * GetTime()) > 0;
    if (state.selecting || blink) {
        DrawRectangle(state.textbox_padding + caret_x, state.textbox_padding + caret_y, caret_w, state.char_size.y, BLACK);
    }
}

void LrsDrawBottomBar(LrsState state) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    DrawRectangle(0, h - 32, w, h, LRS_GREEN);
    int max_char_length = 0;
    for (int i = 0; i < state.line_count; i++) {
        if (state.lines[i].char_length > max_char_length) {
            max_char_length = state.lines[i].char_length;
        }
    }
    DrawTextEx(state.font, TextFormat("%s", state.selecting ? "selecting" : "not selecting"), (Vector2) { 4, h - 28 }, 24, SPACING, WHITE);
}

int main() {
	InitWindow(600, 400, "Letters");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);
    SetTextLineSpacing(0);

    LrsState state = {
        .font = LoadFontEx("assets/DMMono-Regular.ttf", 20, 0, 250),
        .font_size = 20,
        .textbox_padding = 4,
        .dirty = true,
        .selecting = false,
        .timing_offset = 0,
        .text = "Ciaò!",
        .mouse_pos = (Vector2){ 0, 0 },
        .mouse_char_pos = (IntVec2){ 0, 0 },
        .caret_offset = 0,
        .preferred_line_char = 0,
        .textbox_rect = (Rectangle){ 0 },
    };

	while (!WindowShouldClose())
	{
        LrsUpdateState(&state);

		BeginDrawing();

		ClearBackground(RAYWHITE);
        LrsDrawTextBox(state);
        LrsDrawBottomBar(state);
		
		EndDrawing();
	}

	CloseWindow();
}