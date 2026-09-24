# Letters

Letters is a work in progress basic text editor built using C and Raylib.

To build this project, you need:
- a C compiler that supports C99;
- Raylib installed on your system (see [this guide](https://example.com)).

On MacOS, you can use the following command to compile:

```bash
mkdir -p out && cc -std=c99 main.c $(pkg-config --libs --cflags raylib) -o out/Letters
```

# Progress

- [x] Basic UTF8 character displaying and rendering
- [ ] Multi-character selection with mouse and keyboard
- [ ] Handling actions on selection (replace, delete)
- [ ] Supporting copy/paste
- [ ] Read/write from/to files
- [ ] Undo/redo
- [ ] Find and replace
