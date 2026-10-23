#include "raylib.h"
#include "lib/letters.h"
#include "lib/ds.h"

//#define TEST

int main() {
	#ifdef TEST
	LrsState state = LrsGetInitialState();
	LrsUpdateState(&state);
	for (int i = 0; i < state.line_count; i++) {
		printf("Line: %d, start: %d, length: %d\n", i + 1, state.lines[i].start, state.lines[i].length);
	}
	printf("Offset: %d, line: %d\n", 60, LrsGetLineIndexFromPos(state, 60));
	printf("Offset: %d, line: %d\n", 59, LrsGetLineIndexFromPos(state, 59));
	printf("Offset: %d, line: %d\n", 61, LrsGetLineIndexFromPos(state, 61));
	return 0;
	#else
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
	InitWindow(600, 400, "Letters");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);
  SetTextLineSpacing(0);

  LrsState state = LrsGetInitialState();

	while (!WindowShouldClose()) {
    LrsUpdateState(&state);

		BeginDrawing();

		ClearBackground(RAYWHITE);
    LrsDrawTextBox(state);
    LrsDrawBottomBar(state);
		
		EndDrawing();
	}

	CloseWindow();
	#endif
}