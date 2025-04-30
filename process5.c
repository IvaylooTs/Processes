#define _XOPEN_SOURCE 700 // For pselect, fd_set, etc.
#include "common.h"
#include <signal.h>
#include <sys/select.h>
#include <mqueue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h> // For O_NONBLOCK

volatile sig_atomic_t terminate_flag = 0;
FILE *log_fp = NULL;

// IPC Descriptors
int fifo_fd = -1;
mqd_t mq_desc = (mqd_t)-1;
int listen_sock_fd = -1;
int client_sock_fd = -1; // Only one client (P4) expected

// IPC Paths/Names
const char *fifo_path = FIFO_PATH;
const char *mq_name = MQ_NAME;
const char *socket_path = SOCKET_PATH;

void sigterm_handler(int signum) {
    (void)signum; // Explicitly mark signum as unused
    terminate_flag = 1;
}

// Function to clean up resources
void cleanup() {
    printf("\nProcess 5 (PID: %d) Cleaning up...\n", getpid());

    if (log_fp) {
        fflush(log_fp); // Ensure all data is written
        fclose(log_fp);
        log_fp = NULL;
    }

    // Close and unlink IPC resources
    if (fifo_fd != -1) {
        close(fifo_fd);
        unlink(fifo_path); // Remove FIFO from filesystem
        fifo_fd = -1;
    }
    if (mq_desc != (mqd_t)-1) {
        mq_close(mq_desc);
        mq_unlink(mq_name); // Remove message queue
        mq_desc = (mqd_t)-1;
    }
    if (client_sock_fd != -1) {
        close(client_sock_fd);
        client_sock_fd = -1;
    }
    if (listen_sock_fd != -1) {
        close(listen_sock_fd);
        unlink(socket_path); // Remove socket file
        listen_sock_fd = -1;
    }
    printf("\nProcess 5 (PID: %d) Finished.\n", getpid());
    fflush(stdout);
}


// Make descriptor non-blocking
int make_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL O_NONBLOCK)");
        return -1;
    }
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s <log_filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *log_filename = argv[1];

    printf("\nProcess 5 (PID: %d) Started. Logging to: %s\n", getpid(), log_filename);
    fflush(stdout);

    // Register signal handler *before* creating resources
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &action, NULL);

    // Setup cleanup function on exit/termination
    atexit(cleanup); // Register cleanup to be called on normal exit
    // Signal handler sets flag, main loop checks flag and calls cleanup


    // Open log file in append mode
    log_fp = fopen(log_filename, "a");
    if (!log_fp) {
        perror("\nProcess 5: Failed to open log file\n");
        exit(EXIT_FAILURE);
    }
    if (setvbuf(log_fp, NULL, _IOLBF, 0) != 0) {
        perror("\nProcess 5: Failed to set line buffering\n");
        // Decide if this is fatal, maybe exit?
        // exit(EXIT_FAILURE); // Optional: exit if buffering fails
    }


    // --- Set up IPC mechanisms ---

    // 1. FIFO (for P2)
    if (mkfifo(fifo_path, 0666) == -1) {
        if (errno != EEXIST) { // Ignore error if FIFO already exists
            perror("\nProcess 5: Failed to create FIFO\n");
            exit(EXIT_FAILURE); // Use atexit cleanup
        }
    }
    fifo_fd = open(fifo_path, O_RDONLY | O_NONBLOCK); // Non-blocking read
    if (fifo_fd == -1) {
        perror("\nProcess 5: Failed to open FIFO\n");
        exit(EXIT_FAILURE);
    }
    if (make_non_blocking(fifo_fd) == -1) exit(EXIT_FAILURE);

    // 2. Message Queue (for P3)
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAX_MSGS;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;
    // Open/Create MQ with non-blocking flag
    mq_desc = mq_open(mq_name, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
    if (mq_desc == (mqd_t)-1) {
        perror("\nProcess 5: Failed to create/open message queue\n");
        if (errno == EEXIST) {
            // Try opening without create if it exists but failed before
            mq_desc = mq_open(mq_name, O_RDONLY | O_NONBLOCK);
            if (mq_desc == (mqd_t)-1){
                perror("\nProcess 5: Failed to open existing message queue\n");
                exit(EXIT_FAILURE);
            }
        } else {
            exit(EXIT_FAILURE);
        }
    }

    // 3. Unix Domain Socket (for P4)
    listen_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_sock_fd == -1) {
        perror("\nProcess 5: Failed to create socket\n");
        exit(EXIT_FAILURE);
    }
    if (make_non_blocking(listen_sock_fd) == -1) exit(EXIT_FAILURE);


    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path); // Remove old socket file if it exists

    if (bind(listen_sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("\nProcess 5: Failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock_fd, 1) == -1) { // Listen for 1 connection (P4)
        perror("\nProcess 5: Failed to listen on socket\n");
        exit(EXIT_FAILURE);
    }

    printf("\nProcess 5: IPC mechanisms initialized. Waiting for data...\n");
    fflush(stdout);


    // --- Main Event Loop using select ---
    fd_set read_fds;
    int max_fd;
    char buffer[MAX_MSG_SIZE];
    struct timespec select_timeout = {1, 0}; // 1 second timeout for select

    while (!terminate_flag) {
        FD_ZERO(&read_fds);
        max_fd = 0;

        // Add FIFO fd
        FD_SET(fifo_fd, &read_fds);
        if (fifo_fd > max_fd) max_fd = fifo_fd;

        // Add Listening socket fd (to accept new connection from P4)
        FD_SET(listen_sock_fd, &read_fds);
        if (listen_sock_fd > max_fd) max_fd = listen_sock_fd;


        // Add Client socket fd (if connected)
        if (client_sock_fd != -1) {
            FD_SET(client_sock_fd, &read_fds);
            if (client_sock_fd > max_fd) max_fd = client_sock_fd;
        }

        // Add Message Queue - select() doesn't work directly on POSIX MQs.
        // We'll check it non-blockingly after select returns.

        // Wait for activity or timeout
        // Use pselect to handle signals safely during select
        sigset_t empty_mask;
        sigemptyset(&empty_mask);
        int activity = pselect(max_fd + 1, &read_fds, NULL, NULL, &select_timeout, &empty_mask);

        // Reset timeout for next iteration (pselect might modify it)
        select_timeout.tv_sec = 1;
        select_timeout.tv_nsec = 0;


        if (activity == -1) {
            if (errno == EINTR) { // Interrupted by signal (SIGTERM likely)
                continue; // Loop will check terminate_flag
            }
            perror("\nProcess 5: pselect error\n");
            terminate_flag = 1; // Terminate on other select errors
            continue;
        }

        // Check for termination signal *after* select returns
        if (terminate_flag) {
            break;
        }

        // --- Handle IPC activity ---

        // 1. Check Listening Socket for new P4 connection
        if (FD_ISSET(listen_sock_fd, &read_fds)) {
            if (client_sock_fd != -1) {
                // Should not happen if only P4 connects, but handle defensively
                printf("\nProcess 5: Ignoring new connection attempt, already connected to P4.\n");
                // Accept and immediately close the new connection? Or just ignore.
                int temp_sock = accept(listen_sock_fd, NULL, NULL);
                if (temp_sock != -1) close(temp_sock);
            } else {
                client_sock_fd = accept(listen_sock_fd, NULL, NULL);
                if (client_sock_fd == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("\nProcess 5: Failed to accept socket connection\n");
                        terminate_flag = 1; // Error accepting
                    }
                    // else: No connection pending right now (EAGAIN/EWOULDBLOCK)
                } else {
                    printf("\nProcess 5: Accepted connection from P4 (socket fd %d).\n", client_sock_fd);
                    fflush(stdout);
                    if (make_non_blocking(client_sock_fd) == -1) {
                        terminate_flag = 1; // Error setting non-blocking
                    }
                }
            }
        }


        // 2. Check Client Socket (P4) for data
        if (client_sock_fd != -1 && FD_ISSET(client_sock_fd, &read_fds)) {
            ssize_t bytes_read = read(client_sock_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0'; // Null-terminate
                fprintf(log_fp, "%s", buffer); // Already includes "4: ..." and newline
                // fflush(log_fp); // Line buffering should handle this
                printf("\nProcess 5: Received from P4: %s\n", buffer);
                display_menu();// Log to console too
                fflush(stdout);
            } else if (bytes_read == 0) {
                // Connection closed by P4
                printf("\nProcess 5: P4 closed socket connection (fd %d).\n", client_sock_fd);
                fflush(stdout);
                close(client_sock_fd);
                client_sock_fd = -1;
            } else { // bytes_read == -1
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("\nProcess 5: Error reading from client socket\n");
                    close(client_sock_fd); // Close on error
                    client_sock_fd = -1;
                }
                // else: No data available right now (EAGAIN/EWOULDBLOCK)
            }
        }

        // 3. Check FIFO (P2) for data
        if (FD_ISSET(fifo_fd, &read_fds)) {
            ssize_t bytes_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0'; // Null-terminate
                fprintf(log_fp, "%s", buffer); // Already includes "2: ..." and newline
                // fflush(log_fp);
                printf("\nProcess 5: Received from P2: %s\n", buffer);
                display_menu();
                fflush(stdout);

                // Re-open FIFO for reading if P2 closed its writing end?
                // No, the read end stays open. We just get EOF (read returns 0)
                // when all writers close. Let's handle EOF below.

            } else if (bytes_read == 0) {
                // EOF on FIFO - P2 process likely terminated and closed write end.
                // We need to reopen the FIFO to accept new connections if P2 restarts.
                printf("\nProcess 5: P2 closed FIFO write end. Reopening read end.\n");
                fflush(stdout);
                close(fifo_fd);
                fifo_fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
                if (fifo_fd == -1) {
                    perror("\nProcess 5: Failed to reopen FIFO after P2 close\n");
                    terminate_flag = 1; // Cannot continue without FIFO
                } else {
                    if (make_non_blocking(fifo_fd) == -1) terminate_flag = 1;
                }

            } else { // bytes_read == -1
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("\nProcess 5: Error reading from FIFO\n");
                    terminate_flag = 1; // Terminate on unexpected FIFO error
                }
                // else: No data available right now (EAGAIN/EWOULDBLOCK)
            }
        }


        // 4. Check Message Queue (P3) non-blockingly
        ssize_t mq_bytes_read;
        do {
            mq_bytes_read = mq_receive(mq_desc, buffer, MAX_MSG_SIZE, NULL); // Use MAX_MSG_SIZE as buffer size
            if (mq_bytes_read >= 0) {
                buffer[mq_bytes_read] = '\0'; // Null-terminate received message
                fprintf(log_fp, "%s\n", buffer); // Add newline, msg doesn't have it
                // fflush(log_fp);
                printf("\nProcess 5: Received from P3: %s\n", buffer);
                display_menu();
                fflush(stdout);
            } else { // mq_bytes_read == -1
                if (errno != EAGAIN) { // EAGAIN means queue is empty (expected)
                    perror("\nProcess 5: mq_receive error\n");
                    // Consider if this error is fatal
                    // terminate_flag = 1;
                    break; // Exit the mq read loop for this select cycle
                }
                // else: Queue is empty, break the loop
                break;
            }
        } while (mq_bytes_read >= 0 && !terminate_flag); // Keep reading if messages are available


    } // End while (!terminate_flag)

    // Cleanup is handled by atexit or explicit call if needed
    // cleanup(); // atexit should cover normal termination path

    return 0;
}