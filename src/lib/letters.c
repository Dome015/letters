#include "letters.h"

#define SPACING 0
#define FONT_SIZE 18

#define LRS_GREEN ((Color){ 55, 117, 0, 255 })

LrsState LrsGetInitialState() {
    return (LrsState) {
        .font = LoadFontEx("assets/DMMono-Regular.ttf", FONT_SIZE * 2, 0, 250),
        .font_size = FONT_SIZE,
        .textbox_padding = 4,
        .dirty = true,
        .selecting = false,
        .timing_offset = 0,
        .text = "Rido ma piango di gusto,\nse vedo il bell'imbusto palestrato\ne lampadato al punto giusto!",
        .mouse_pos = (Vector2){ 0, 0 },
        .mouse_char_pos = (IntVec2){ 0, 0 },
        .caret_offset = 0,
        .selection_start_offset = -1,
        .selection_offset_range = (IntRange){ 0, 0 },
        .preferred_line_char = 0,
        .textbox_rect = (Rectangle){ 0 },
        .blink_start = GetTime()
    };
}

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

// Replaces the bytes at the provided offset with the provided ones
void LrsReplaceBytes(LrsState* state, int offset, int replaced_size, char* bytes, int size) {
    // If the replaced size is greater than the replacement, we need to shift the following
    // bytes backwards; otherwise, we need to shift them forward
    int replace_end_offset = offset + replaced_size;
    if (replaced_size > size) {
        int shift_count = replaced_size - size;
        for (int i = replace_end_offset - shift_count; i <= state->byte_count - shift_count; i++) {
            state->text[i] = state->text[i + shift_count];
        }
    } else {
        int shift_count = size - replaced_size;
        for (int i = state->byte_count + shift_count; i >= replace_end_offset + shift_count; i--) {
            state->text[i] = state->text[i - shift_count];
        }
    }
    // Insert the new text
    for (int i = 0; i < size; i++) {
        state->text[offset + i] = bytes[i];
    }
    // Update the byte count
    state->byte_count += size - replaced_size;
    // Compute the new char count
    int replaced_char_count = LrsUTF8CharCount(&(state->text[offset]), replaced_size);
    int replacement_char_count = LrsUTF8CharCount(bytes, size);
    state->char_count += replacement_char_count - replaced_char_count;
    state->dirty = true;
}

// Replaces the current selection with the provided bytes, given their size.
// If the selection is empty, it inserts the bytes at the current caret position.
void LrsReplaceSelection(LrsState* state, char* bytes, int size) {
    LrsReplaceBytes(
        state,
        state->selection_offset_range.start,
        state->selection_offset_range.end - state->selection_offset_range.start,
        bytes,
        size);
}

// Copy the bytes from the given offset up to the given size to the clipboard.
void LrsCopyBytes(LrsState* state, int offset, int size) {
    char replaced = state->text[offset + size];
    state->text[offset + size] = '\0';
    SetClipboardText(&state->text[offset]);
    state->text[offset + size] = replaced;
}

// Move cate to the specified target offset. If the flag is activated, the current selection is
// extended to the new caret position.
void LrsMoveCaret(LrsState* state, int target_offset, bool set_selection) {
    // Reset blinking animation
    state->blink_start = GetTime();
    if (target_offset < 0 || target_offset > state->byte_count) {
        return;
    }
    if (!set_selection) {
        // Reset the selection to an empty one pointing to the new offset
        // and set the new caret offset
        state->selection_start_offset = target_offset;
        state->selection_offset_range.start = target_offset;
        state->selection_offset_range.end = target_offset;
        state->caret_offset = target_offset;
        return;
    }
    // If we're setting the selection, and beginning a new one (current range is empty),
    // set the start offset for the selection
    if (state->selection_offset_range.start == state->selection_offset_range.end) {
        state->selection_start_offset = state->caret_offset;
    }
    // Update the range
    if (target_offset < state->selection_start_offset) {
        state->selection_offset_range.start = target_offset;
        state->selection_offset_range.end = state->selection_start_offset;
    } else {
        state->selection_offset_range.start = state->selection_start_offset;
        state->selection_offset_range.end = target_offset;
    }
    state->caret_offset = target_offset;
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

// Checks whether a key is being pressed (either for the first time or held down).
bool LrsIsKeyPressed(int key) {
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

// Handles key inputs (DEL/BACKSPACE/ARROW KEYS...) and updates the state accordingly
void LrsHandleKeyInput(LrsState* state) {
    // For now, we use mac-specific key bindings for text navigation. Windows support could come
    // in the future.
    bool shift_pressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrl_pressed = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool cmd_pressed = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    int pressed_key;
    bool selection_present = state->selection_offset_range.end - state->selection_offset_range.start > 0;
    // We need to keep track of whether a certain key is being held down for long. If that's the case,
    // it is equivalent to pressing it repeatedly.
    if (cmd_pressed && LrsIsKeyPressed(KEY_LEFT)) {
        LineInfo caret_line = LrsGetLineFromPos(*state, state->caret_offset);
        int first_non_whitespace_offset = 0;
        while (isspace(state->text[caret_line.start + first_non_whitespace_offset]) && first_non_whitespace_offset < caret_line.length) {
            first_non_whitespace_offset += 1;
        }
        // In case the line is either empty or contains no non-whitespace characters,
        // we only want to travel to its start.
        if (first_non_whitespace_offset == caret_line.length) {
            first_non_whitespace_offset = 0;
        }
        int target_offset = caret_line.start + first_non_whitespace_offset;
        // If the caret is already at the first non-whitespace character, move it to
        // the beginning of the line, and viceversa.
        if (state->caret_offset == target_offset) {
            LrsMoveCaret(state, caret_line.start, shift_pressed);
        } else {
            LrsMoveCaret(state, target_offset, shift_pressed);
        }
    } else if (LrsIsKeyPressed(KEY_LEFT) || (ctrl_pressed && LrsIsKeyPressed(KEY_B))) {
        bool start_reached = state->caret_offset == 0;
        int char_size = start_reached ? 0 : LrsUTF8PrevCharSize(state->text, state->caret_offset);
        int target_offset = (selection_present && !shift_pressed) ?
            state->selection_offset_range.start : state->caret_offset - char_size;
        LrsMoveCaret(state, target_offset, shift_pressed);
    }
    if (cmd_pressed && LrsIsKeyPressed(KEY_RIGHT)) {
        LineInfo caret_line = LrsGetLineFromPos(*state, state->caret_offset);
        int target_offset = caret_line.start + caret_line.length;
        if (state->caret_offset != target_offset) {
            LrsMoveCaret(state, target_offset, shift_pressed);
        }
    } else if (LrsIsKeyPressed(KEY_RIGHT) || (ctrl_pressed && LrsIsKeyPressed(KEY_F))) {
        bool end_reached = state->caret_offset == state->lines[state->line_count - 1].start + state->lines[state->line_count - 1].length;
        int char_size = end_reached ? 0 : LrsUTF8CharSize(state->text[state->caret_offset]);
        int target_offset = (selection_present && !shift_pressed) ?
            state->selection_offset_range.end : state->caret_offset + char_size;
        LrsMoveCaret(state, target_offset, shift_pressed);
        LineInfo caret_line = LrsGetLineFromPos(*state, state->caret_offset);
        int caret_line_offset = state->caret_offset - caret_line.start;
        state->preferred_line_char = caret_line_offset;
    }
    if (LrsIsKeyPressed(KEY_UP) || (ctrl_pressed && LrsIsKeyPressed(KEY_P))) {
        int offset = shift_pressed ? state->caret_offset : state->selection_offset_range.start;
        int offset_line_index = LrsGetLineIndexFromPos(*state, offset);
        LineInfo offset_line = state->lines[offset_line_index];
        int offset_line_offset = offset - offset_line.start;
        int offset_line_char = LrsGetLineCharCountFromOffset(state->text, offset_line, offset_line_offset);
        int target_offset = 0;
        if (offset_line_index != 0) {
            LineInfo prev_line = state->lines[offset_line_index - 1];
            if (prev_line.char_length >= offset_line_char) {
                target_offset = prev_line.start + LrsGetLineOffsetFromCharCount(state->text, prev_line, offset_line_char);
            } else {
                state->preferred_line_char = offset_line_char;
                target_offset = offset_line.start - 1;
            }
        }
        LrsMoveCaret(state, target_offset, shift_pressed);
    }
    if (LrsIsKeyPressed(KEY_DOWN) || (ctrl_pressed && LrsIsKeyPressed(KEY_N))) {
        int caret_line_index = LrsGetLineIndexFromPos(*state, state->caret_offset);
        LineInfo caret_line = state->lines[caret_line_index];
        int caret_line_offset = state->caret_offset - caret_line.start;
        int caret_line_char = LrsGetLineCharCountFromOffset(state->text, caret_line, caret_line_offset);
        int target_offset = caret_line.start + caret_line.length;
        if (caret_line_index != state->line_count - 1) {
            LineInfo next_line = state->lines[caret_line_index + 1];
            if (next_line.char_length >= caret_line_char) {
                target_offset = next_line.start + LrsGetLineOffsetFromCharCount(state->text, next_line, caret_line_char);
            } else {
                state->preferred_line_char = caret_line_char;
                target_offset = next_line.start + next_line.length;
            }
        }
        LrsMoveCaret(state, target_offset, shift_pressed);
    }
    if (LrsIsKeyPressed(KEY_BACKSPACE)) {
        if (state->caret_offset != 0) {
            int deleted_bytes = LrsDeleteCharsBack(state, state->caret_offset, 1);
            LrsMoveCaret(state, state->caret_offset - deleted_bytes, false);
        }
    }
    if (LrsIsKeyPressed(KEY_DELETE)) {
        if (state->caret_offset == state->byte_count) {
            LrsDeleteCharsForward(state, state->caret_offset, 1);
        }
    }
    if (LrsIsKeyPressed(KEY_ENTER)) {
        LrsInsertBytes(state, state->caret_offset, "\n", 1);
        LrsMoveCaret(state, state->caret_offset + 1, false);
    }
    if (cmd_pressed && LrsIsKeyPressed(KEY_C)) {
        LrsCopyBytes(
            state,
            state->selection_offset_range.start,
            state->selection_offset_range.end - state->selection_offset_range.start);
    }
    if (cmd_pressed && LrsIsKeyPressed(KEY_X)) {
        int target_offset = state->selection_offset_range.start;
        LrsCopyBytes(
            state,
            state->selection_offset_range.start,
            state->selection_offset_range.end - state->selection_offset_range.start);
        LrsReplaceSelection(state, NULL, 0);
        LrsMoveCaret(state, target_offset, false);
    }
    if (cmd_pressed && LrsIsKeyPressed(KEY_V)) {
        const char* clipboard_text = GetClipboardText();
        int size = strlen(clipboard_text);
        LrsReplaceSelection(state, clipboard_text, size);
        LrsMoveCaret(state, state->selection_offset_range.start + size, false);
    }
    if (cmd_pressed && LrsIsKeyPressed(KEY_A)) {
        state->selection_start_offset = 0;
        state->selection_offset_range = (IntRange){ 0, state->byte_count };
        state->caret_offset = state->byte_count;
    }
    if (LrsIsKeyPressed(KEY_F1)) {
        // Test key, used to mess around
        LrsReplaceBytes(state, state->selection_offset_range.start, state->selection_offset_range.end - state->selection_offset_range.start, "pippò", 6);
        LrsMoveCaret(state, state->selection_offset_range.start + 5, false);
    }
}

// Handle unicode character input and update the state accordingly.
void LrsHandleCharInput(LrsState* state) {
    int pressed_char;
    while ((pressed_char = GetCharPressed()) != 0) {
        char to_add[4];
        int size = LrsUTF8Encode(pressed_char, to_add);
        LrsReplaceSelection(state, to_add, size);
        LrsMoveCaret(state, state->selection_offset_range.start + size, false);
    }
}

// Handle mouse input and update the state accordingly.
void LrsHandleMouseInput(LrsState* state) {
    bool shift_pressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    // Mouse click to move the caret to a point
    if (state->mouse_char_pos.x != -1) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int mouse_offset = LrsGetOffsetFromCharPos(*state, state->mouse_char_pos);
            LrsMoveCaret(state, mouse_offset, state->selecting || shift_pressed);
            if (!state->selecting) {
                state->selecting = true;
            }
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state->selecting = false;
    }
}

// Update the editor state
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
    // Update selection
    if (state->selecting) {
        int mouse_offset = LrsGetOffsetFromCharPos(*state, state->mouse_char_pos);
        if (mouse_offset < state->selection_start_offset) {
            state->selection_offset_range = (IntRange){ mouse_offset, state->selection_start_offset };
        } else {
            state->selection_offset_range = (IntRange){ state->selection_start_offset, mouse_offset };
        }
    }
    // INPUT HANDLING
    LrsHandleMouseInput(state);
    // Handle pressed keys
    LrsHandleKeyInput(state);
    // Type characters
    LrsHandleCharInput(state);
}

// Draw the text box of the editor
void LrsDrawTextBox(LrsState state) {
    // Selection
    int padding = state.textbox_padding;
    if ((state.selection_offset_range.end - state.selection_offset_range.start) > 0) {
        int start_line_index = LrsGetLineIndexFromPos(state, state.selection_offset_range.start);
        int end_line_index = LrsGetLineIndexFromPos(state, state.selection_offset_range.end);
        for (int i = start_line_index; i <= end_line_index; i++) {
            LineInfo line = state.lines[i];
            int start_char = i == start_line_index ?
                LrsGetLineCharCountFromOffset(state.text, line, state.selection_offset_range.start - line.start) :
                0;
            int end_char = i == end_line_index ?
                LrsGetLineCharCountFromOffset(state.text, line, state.selection_offset_range.end - line.start) :
                line.char_length + 1;
            
            DrawRectangle(
                padding + state.char_size.x * start_char,
                padding + state.char_size.y * i,
                state.char_size.x * (end_char - start_char),
                state.char_size.y, LIGHTGRAY);
        }
    }
    // Text
    DrawTextEx(state.font, state.text, (Vector2) { padding, padding }, state.font_size, 0, BLACK);
    // Caret
    int line_number = LrsGetLineIndexFromPos(state, state.caret_offset);
    int caret_y = state.char_size.y * line_number;
    int caret_char_offset = LrsGetLineCharCountFromOffset(state.text, state.lines[line_number], state.caret_offset - state.lines[line_number].start);
    int caret_x = state.char_size.x * caret_char_offset;
    int caret_w = state.char_size.x * 0.4;
    bool blink = sin(8 * (GetTime() - state.blink_start)) > 0;
    if (state.selecting || blink) {
        DrawRectangle(state.textbox_padding + caret_x, state.textbox_padding + caret_y, caret_w, state.char_size.y, BLACK);
    }
}

// Draw the bottom bar below the text box
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
    DrawTextEx(state.font, TextFormat("Caret offset: %d", state.caret_offset), (Vector2) { 4, h - 28 }, 24, SPACING, WHITE);
}