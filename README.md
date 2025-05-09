# Smash - Small Shell Implementation
A lightweight, custom shell implementation in C++ that mimics basic bash functionality on Linux systems.
# Overview
Smash (Small Shell) is a command-line interpreter implemented in C++ that runs on Linux Ubuntu 18 64-bit. It provides many of the basic functionalities of a standard bash shell, using direct system calls for process management and command execution.
# Features

Command execution with arguments
Built-in commands:

cd - Change directory
pwd - Print working directory
jobs - Display list of running background jobs
kill - Send signals to processes
fg - Bring background job to foreground
bg - Resume job in background
quit - Exit the shell


I/O redirection (>, >>)
Piping between commands (|)
Background process execution (&)
Signal handling (SIGINT, SIGTSTP, etc.)
Command history navigation

# Project Structure

smash.cpp: Main loop of the shell program waiting for and parsing commands
Commands.cpp/h: Implementation of all supported commands
Signals.cpp/h: Signal handling functionality

# Installation
Prerequisites

Linux Ubuntu 18.04 or compatible (64-bit)
GCC compiler (with C++11 support)
Make

Building the Shell

Clone the repository:

bashgit clone https://github.com/yourusername/smash.git
cd smash

Compile the project:

bashmake
# Usage
Running the Shell
bash./smash
Basic Commands
smash> ls -la                      # Run external commands with arguments
smash> cd /path/to/directory       # Change current directory
smash> pwd                         # Print working directory
smash> sleep 10 &                  # Run a process in the background
smash> jobs                        # Show background jobs
smash> kill -9 <job-id>            # Send SIGKILL to a job
smash> fg <job-id>                 # Bring job to foreground
smash> bg <job-id>                 # Continue job in background
smash> ls > output.txt             # Redirect output to file
smash> ls >> output.txt            # Append output to file
smash> ls | grep txt               # Pipe commands
smash> quit                        # Exit the shell
Signal Handling
The shell handles several signals:

SIGINT (Ctrl+C): Terminates the foreground process
SIGTSTP (Ctrl+Z): Stops the foreground process
SIGCONT: Continues a stopped process

# Troubleshooting

If you encounter "Permission denied" errors, make sure the smash executable has execute permissions:
bashchmod +x smash

For any segmentation faults, ensure your system is compatible with the minimal requirements.

# Contributing
This project was developed by me, Tomer Weiser and my partner, Ofir Cohen. Contributions are welcome through pull requests.
# License
This project is licensed under the MIT License - see the LICENSE file for details.
