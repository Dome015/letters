#include "raylib.h"
#include "lib/letters.h"
#include "lib/ds.h"

int main() {
	InitWindow(600, 400, "Letters");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);
    SetTextLineSpacing(0);

    LrsState state = LrsGetInitialState();

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