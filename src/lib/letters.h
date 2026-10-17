#ifndef __LETTERS_H__
#define __LETTERS_H__

#include "raylib.h"
#include "ds.h"
#include "utf8.h"

#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

typedef struct {
    int start;
    int length;
    int char_start;
    int char_length;
} LineInfo;

typedef struct LrsState {
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
    IntRange selection_offset_range;
    int selection_start_offset;
    int byte_count;
    int char_count;
    int preferred_line_char;
    Rectangle textbox_rect;
    double blink_start;
} LrsState;

// Creates and initializes a new state, returning it
LrsState LrsGetInitialState();
// Returns the line number corresponding to the specified position, or -1 if the position is not valid
int LrsGetLineIndexFromPos(LrsState state, int offset);
// Returns the line corresponding to the specified position, a placeholder if the position is not valid
LineInfo LrsGetLineFromPos(LrsState state, int offset);
// Given a line info and a number of characters, returns the offset corresponding to the first n characters of that line
int LrsGetLineOffsetFromCharCount(char* text, LineInfo line, int char_count);
// Given a line info and an offset, returns the number of characters corresponding to that offset on that line
int LrsGetLineCharCountFromOffset(char* text, LineInfo line, int offset);
// Given the row an col position of a character, returns the corresponding byte offset
int LrsGetOffsetFromCharPos(LrsState state, IntVec2 char_pos);
// Inserts characters at the given position, updating the state accordingly
void LrsInsertBytes(LrsState* state, int offset, char* bytes, int size);
// Replaces the bytes at the provided offset with the provided ones
void LrsReplaceBytes(LrsState* state, int offset, int replaced_size, char* bytes, int size);
// Move cate to the specified target offset. If the flag is activated, the current selection is
// extended to the new caret position.
void LrsMoveCaret(LrsState* state, int target_offset, bool set_selection);
// Deletes the specified number of VISIBLE characters going forward from the specified offset.
// Returns the number of deleted bytes.
int LrsDeleteCharsForward(LrsState* state, int offset, int count);
// Deletes the specified number of VISIBLE characters going backwards from the specified offset.
// Returns the number of deleted bytes.
int LrsDeleteCharsBack(LrsState* state, int offset, int count);
// Checks whether a key is being pressed (either for the first time or held down).
bool LrsIsKeyPressed(int key);
// Handles key inputs (DEL/BACKSPACE/ARROW KEYS...) and updates the state accordingly
void LrsHandleKeyInput(LrsState* state);
// Handle unicode character input and update the state accordingly.
void LrsHandleCharInput(LrsState* state);
// Handle mouse input and update the state accordingly.
void LrsHandleMouseInput(LrsState* state);
// Update the editor state
void LrsUpdateState(LrsState* state);
// Draw the text box of the editor
void LrsDrawTextBox(LrsState state);
// Draw the bottom bar below the text box
void LrsDrawBottomBar(LrsState state);

#endif