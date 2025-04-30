#define _POSIX_C_SOURCE 200809L // For nanosleep
#include "common.h"
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

volatile sig_atomic_t terminate_flag = 0;
char text_color_code[10] = COL_WHITE;
char bg_color_code[10] = BG_BLACK;

void sigterm_handler(int signum) {
    (void)signum; // Explicitly mark signum as unused
    terminate_flag = 1;
}

void set_colors() {
    printf("%s%s", text_color_code, bg_color_code);
}

void reset_colors() {
    printf("%s", COL_RESET);
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "\nUsage: %s <text_color_code> <bg_color_code> <pause_ms> <socket_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    snprintf(text_color_code, sizeof(text_color_code), "\x1B[%sm", argv[1]);
    snprintf(bg_color_code, sizeof(bg_color_code), "\x1B[%sm", argv[2]);

    long pause_ms_long = strtol(argv[3], NULL, 10);
    if (pause_ms_long <= 0) {
        fprintf(stderr, "\nInvalid pause time (must be > 0).\n");
        exit(EXIT_FAILURE);
    }
    struct timespec pause_duration = {pause_ms_long / 1000, (pause_ms_long % 1000) * 1000000};

    const char *socket_path = argv[4];
    int sock_fd;
    struct sockaddr_un server_addr;
    char input_buffer[MAX_MSG_SIZE - 10]; // Leave space for prefix "4: \n"
    char send_buffer[MAX_MSG_SIZE];

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &action, NULL);
    // Ignore SIGPIPE, handle write errors instead
    signal(SIGPIPE, SIG_IGN);


    set_colors();
    printf("\nProcess 4 (PID: %d) Started. Reading Strings. Socket: %s\n", getpid(), socket_path);
    reset_colors();
    fflush(stdout);

    // Create socket
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        set_colors();
        perror("\nProcess 4: Failed to create socket\n");
        reset_colors();
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    // Connect to P5's socket
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        set_colors();
        perror("\nProcess 4: Failed to connect to socket\n");
        reset_colors();
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    while (!terminate_flag) {
        set_colors();
        struct timespec extra_delay = {0, 1 * 1000000}; // Example: 100ms extra delay
        nanosleep(&extra_delay, NULL);
        printf("\nProcess 4: Enter a string: ");
        reset_colors();
        fflush(stdout);

        if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
            // Remove trailing newline if present
            input_buffer[strcspn(input_buffer, "\n")] = 0;

            snprintf(send_buffer, sizeof(send_buffer), "4: %s\n", input_buffer);

            if (send(sock_fd, send_buffer, strlen(send_buffer), 0) == -1) {
                if (errno == EPIPE) {
                    set_colors();
                    fprintf(stderr, "\nProcess 4: Socket connection closed by P5. Terminating.\n");
                    reset_colors();
                    terminate_flag = 1; // Stop the loop
                } else {
                    set_colors();
                    perror("\nProcess 4: Failed to send data\n");
                    reset_colors();
                    terminate_flag = 1; // Terminate on other send errors too
                }
            } else {
                nanosleep(&pause_duration, NULL);
            }

        } else {
            // Handle input error or EOF
            if (feof(stdin)) {
                set_colors();
                printf("\nProcess 4: EOF detected on input. Terminating.\n");
                reset_colors();
            } else {
                set_colors();
                perror("\nProcess 4: Error reading input\n");
                reset_colors();
            }
            terminate_flag = 1; // Stop loop on input error/EOF
        }
        fflush(stdout);
    }

    // Cleanup
    close(sock_fd);
    set_colors();
    printf("\nProcess 4 (PID: %d) Finishing.\n", getpid());
    reset_colors();
    fflush(stdout);

    return 0;
}