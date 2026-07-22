# notfm

>[!NOTE]
> This is just my college project. I was not planning to add it to GitHub. It is here only as a backup.

## Features

- Three-pane terminal interface for parent, current directory, and previews
- Vim-style navigation with arrow-key support
- Fuzzy file search
- Create, rename, copy, cut, paste, and delete files or directories
- Toggle hidden files
- Open files with the system default application
- Text, directory, image, and video previews
- Background thumbnail generation and caching for images and videos

## Building

### Requirements

- A C23-compatible compiler
- CMake 3.12 or newer
- `pkg-config`
- Notcurses development files
- libvips development files

Build the project with CMake:

```sh
cmake -S . -B build
cmake --build build
```

Run it from the project directory, optionally providing a starting path:

```sh
./build/notfm [path]
```

For enhanced previews, install `bat` or `highlight` for text files and
`ffmpegthumbnailer` for video thumbnails.

## Command Prompt

Press `:` to open the command prompt, then use one of these commands:

| Command | Action |
| --- | --- |
| `q` | Quit notfm |
| `add <name>` | Create a file in the current directory |
| `add <name>/` | Create a directory in the current directory |
| `rename <name>` | Rename the selected entry |
| `delete` | Request deletion of the selected entry; confirm with `y` |
| `find <query>` | Fuzzy-search the current directory |
| `paste` | Paste the copied or cut entry into the current directory |

## Keybindings

| Key | Action |
| --- | --- |
| `j` / `↓` | Move selection down |
| `k` / `↑` | Move selection up |
| `h` / `←` / `Backspace` | Go to the parent directory |
| `l` / `→` / `Enter` | Open the selected directory or file |
| `/` | Start a fuzzy search |
| `n` / `N` | Next / previous search result |
| `a` | Add a file or directory |
| `r` | Rename the selected entry |
| `yy` | Copy the selected entry |
| `x` | Cut the selected entry |
| `p` | Paste the copied or cut entry |
| `dd` | Delete the selected entry (with confirmation) |
| `z` | Toggle hidden files |
| `:` | Open the command prompt |
| `q` | Quit |
