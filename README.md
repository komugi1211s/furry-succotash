# furry-succotash
(wheel reinvent jam participant)

Basic File watcher that runs specified command each time when there's a change in something.

## Usage

*Windows*
requires MSVC.
modify `build.bat` for appropriate SDL / GLFW path and run `build.bat`.

```
./build.bat
```

run `/dist/main.exe`, specify appropriate directory to watch for / process to run.
it fires the specified command whenever detects new file creation / modification. (_TODO: deletion_)


*Unix*
_TODO_
