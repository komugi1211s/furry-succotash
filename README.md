# furry-succotash

(wheel reinvent jam participant)

Basic File watcher that runs specified command each time when there's a change in something.

## Usage

#### Windows

requires MSVC.

modify `build.bat` for appropriate SDL / GLFW path and run `build.bat`.

```
./build.bat
```

run `/dist/main.exe`, specify appropriate directory to watch for / process to run.

it fires the specified command whenever detects new file creation / modification. (_TODO: deletion_)

#### Unix

requires Clang to compile, and Zenity to function properly (for selecting folder / files)

```
./build.sh
```

run `/dist/FurrySuccotash`, specify appropriate directory to watch for / process to run.

it fires the specified command whenever detects new file creation / modification. (_TODO: deletion_)


## TODO
 - reading stdout and logging
 - many folder to multiple command relationship (watch N folder, run M command in parallel / sequentially when there's any kind of change)
 - multiple folder/command pair.
 - resizing
 - minimizing / staying on task bar
 - overlayed logging screen (shows up whenever restart happens and slowly fades away?)
