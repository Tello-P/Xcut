#!/bin/bash

# Define the source file and the output binary name
SOURCE="xcut.c"
BINARY="xcut"

# Check if the source file exists
if [ ! -f "$SOURCE" ]; then
    echo "The file $SOURCE does not exist."
    exit 1
fi

# Ensure that necessary dependencies are installed (X11 library)
echo "Checking for required dependencies..."
sudo apt update

# Install X11 development libraries (if not already installed)
sudo apt install -y libx11-dev gcc

# Compile the C file with the provided flags
gcc -std=c99 -Wall -o "$BINARY" "$SOURCE" -lX11

# Check if the compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful."

    # Define the directory to move the binary to (use ~/bin or other directory in $PATH)
    DEST_DIR="$HOME/bin"

    # Create the directory if it doesn't exist
    if [ ! -d "$DEST_DIR" ]; then
        mkdir -p "$DEST_DIR"
    fi

    # Move the binary to the directory
    mv "$BINARY" "$DEST_DIR/"

    # Ensure the binary is executable
    chmod +x "$DEST_DIR/$BINARY"

    echo "Binary moved to $DEST_DIR and is executable."
else
    echo "There was an error during the compilation."
    exit 1
fi

