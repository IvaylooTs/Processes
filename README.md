# Linux Multi-Process Control and IPC System

This project demonstrates a multi-process system in a Linux environment, showcasing process creation, management, and various Inter-Process Communication (IPC) mechanisms. The system consists of five processes designed to work together, managed by a central menu-driven control process.

## Project Description

The system architecture is based on a main controller process (Process 1) that manages the lifecycle of three worker processes (Process 2, 3, and 4) and a logger process (Process 5). Each worker process is responsible for reading a specific data type from standard input and forwarding it to the logger process using a unique IPC method.

### Key Features

-   **Centralized Process Management:** A single menu-driven application to start and stop worker processes.
-   **Dynamic Configuration:** Common parameters (text/background colors, pause duration) are configured dynamically upon the first worker process launch.
-   **Diverse IPC Mechanisms:**
    -   **Process 2 -> Process 5:** Named Pipe (FIFO) for integer data.
    -   **Process 3 -> Process 5:** POSIX Message Queue for floating-point data.
    -   **Process 4 -> Process 5:** Unix Domain Socket for string data.
-   **Coordinated Lifecycle:** The logger process (Process 5) starts automatically with the first worker and terminates gracefully after the last worker has stopped.
-   **Data Logging:** All data received by the logger process is appended to a user-specified log file, prefixed with the ID of the source process.
-   **Robust I/O Handling:** Use of `stderr` for informational/error messages to keep the user interaction on `stdout` clean.

## Process Functionality

### Process #1: The Controller
This is the main executable that orchestrates the entire system.
-   Provides a menu to start/stop worker processes (P2, P3, P4).
-   Prompts the user for common configuration parameters (colors, pause) on the first worker launch.
-   Manages the lifecycle of the logger process (P5), starting it when needed and stopping it when all workers are terminated.
-   Ensures a clean shutdown by requiring all child processes to be stopped before exiting.

### Process #2: Integer Worker
-   Reads integer values from `stdin`.
-   Sends the data to Process 5 via a **Named Pipe (FIFO)**.
-   Pauses for a specified duration between reads.
-   Customizes its console output color based on parameters from Process 1.

### Process #3: Float Worker
-   Reads floating-point values from `stdin`.
-   Sends the data to Process 5 via a **POSIX Message Queue**.
-   Pauses for a specified duration between reads.
-   Customizes its console output color based on parameters from Process 1.

### Process #4: String Worker
-   Reads string values from `stdin`.
-   Sends the data to Process 5 via a **Unix Domain Socket**.
-   Pauses for a specified duration between reads.
-   Customizes its console output color based on parameters from Process 1.

### Process #5: The Logger
-   A daemon-like process that runs whenever at least one worker is active.
-   Receives data from P2, P3, and P4 through their respective IPC channels.
-   Appends all received data to a log file specified on the command line.
-   Gracefully handles termination signals to ensure all resources are cleaned up (IPC files/queues are unlinked).

## Getting Started

### Prerequisites

-   A Linux-based operating system.
-   A C compiler (e.g., GCC).
-   `make` build automation tool.
-   Core development libraries (`build-essential` on Debian/Ubuntu).

### Compilation

Navigate to the project's root directory in your terminal and run the `make` command. This will compile all five process executables.

```bash
make
```

### Running the System

1.  Execute the main controller process (`process1`), providing a path for the log file as a command-line argument.

    ```bash
    # Example: Log to a file named 'activity.log' in the current directory
    ./process1 activity.log

    # Example: Log to a file in a different directory (ensure the directory exists)
    ./process1 /tmp/my_process_log.txt
    ```

2.  Upon starting the first worker process (by selecting option `1`, `2`, or `3` from the menu), you will be prompted to enter common configuration parameters:
    -   **Text Color:** An ANSI color code (e.g., `31` for Red, `32` for Green, `97` for Bright White).
    -   **Background Color:** An ANSI color code (e.g., `40` for Black, `44` for Blue).
    -   **Pause Time:** A duration in milliseconds (e.g., `500`).

3.  Once the parameters are set, they will be used for all worker processes started during this session.

4.  Interact with the worker processes by entering the requested data type when prompted. Their prompts will appear in the same terminal as the main menu.

5.  Use the menu options (`4`, `5`, `6`) to stop the worker processes.

6.  To exit the system cleanly, first stop all running worker processes and then select option `7` from the menu.
