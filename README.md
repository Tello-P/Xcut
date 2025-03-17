# XCut - Copy Text to X11 Clipboard

**XCut** is a simple and efficient command-line tool that copies the contents of a text file to the X11 clipboard. Unlike similar tools, XCut operates as a background daemon, ensuring the text stays in the clipboard even after the command finishes—until another program takes over clipboard ownership.

## Key Features

- Copies text from a file to the X11 clipboard quickly and efficiently.
- Keeps the content available in the clipboard by running as a daemon.
- Supports `XA_STRING` and `UTF8_STRING` text formats.
- Easy to install and use on Linux systems with X11.

## Requirements

To compile and run XCut, you’ll need:

- **Operating System**: Linux with the X11 window system.
- **Dependencies**:
  - X11 development libraries (`libx11-dev`).
  - GCC compiler (`gcc`).
- **Permissions**: Ability to install dependencies and move the binary to a `$PATH` directory (if using the install script).

## Installation

You can install XCut in two ways: with the provided script or manually.

### Option 1: Using the Installation Script

The `install.sh` script handles dependency checks, compilation, and installation automatically.

1. Ensure `install.sh` and `xcut.c` are in the same directory.
2. Make the script executable:
   ```bash
   chmod +x install.sh
   ```
3. Run the script:
   ```bash
   ./install.sh
   ```
   The script will:
   - Update the system and verify dependencies (`libx11-dev` and `gcc`).
   - Compile `xcut.c` into a binary called `xcut`.
   - Move the binary to `~/bin` (make sure `~/bin` is in your `$PATH`).

   A success message will confirm the installation.

### Option 2: Manual Compilation

For manual installation, follow these steps:

1. Install dependencies:
   ```bash
   sudo apt update
   sudo apt install -y libx11-dev gcc
   ```
2. Compile the source code:
   ```bash
   gcc -std=c99 -Wall -o xcut xcut.c -lX11
   ```
3. Move the binary to a `$PATH` directory (e.g., `~/bin`):
   ```bash
   mv xcut ~/bin/
   ```
4. (Optional) Make it executable:
   ```bash
   chmod +x ~/bin/xcut
   ```

## Usage

Once installed, run XCut with:

```bash
xcut <path_to_file>
```

- **`<path_to_file>`**: The text file whose contents you want to copy.

### Example

To copy the contents of `notes.txt`:
```bash
xcut notes.txt
```

The text will be available in the clipboard for pasting into any X11-supporting application (e.g., text editors or browsers). XCut runs in the background until the clipboard is claimed by another program.

### Stopping the Daemon

XCut stays active until:
- Another program takes clipboard ownership (e.g., by copying new content).
- You manually stop it:
  1. Find the process ID (PID):
     ```bash
     ps aux | grep xcut
     ```
  2. Terminate it:
     ```bash
     kill <PID>
     ```

## Technical Functionality

XCut works as follows:

1. **File Reading**: Loads the file’s contents into memory.
2. **Daemonization**: Runs in the background, freeing your terminal.
3. **Clipboard Management**:
   - Takes ownership of the X11 `CLIPBOARD`.
   - Responds to requests with text in `XA_STRING` or `UTF8_STRING` formats.
4. **Persistence**: Keeps the text available until clipboard ownership is lost.

It uses the Xlib library for X11 interaction, managing memory and events efficiently.

## Additional Notes

- If the file doesn’t exist or no file is specified, XCut will show an error and exit.

## Troubleshooting

- **"Unable to open display"**: Ensure X11 is running (XCut won’t work in a non-graphical terminal).
- **Text not copying**: Check file readability and compilation success.
- **Permission denied**: Verify write permissions in the target directory (e.g., `~/bin`).

## Contact

For questions, suggestions, or bugs, open an issue in the project repository (if available) or contact the developer directly.

---

*XCut - Simple, functional clipboard management for X11.*

---
