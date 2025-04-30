#define _POSIX_C_SOURCE 200809L // For nanosleep
#include "common.h"
#include <signal.h>
#include <time.h>
#include <limits.h> // For INT_MAX, INT_MIN

volatile sig_atomic_t terminate_flag = 0;
char text_color_code[10] = COL_WHITE; // Default colors
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
        fprintf(stderr, "\nUsage: %s <text_color_code> <bg_color_code> <pause_ms> <fifo_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Store colors
    snprintf(text_color_code, sizeof(text_color_code), "\x1B[%sm", argv[1]);
    snprintf(bg_color_code, sizeof(bg_color_code), "\x1B[%sm", argv[2]);

    long pause_ms_long = strtol(argv[3], NULL, 10);
    if (pause_ms_long <= 0) { // Basic validation
        fprintf(stderr, "\nInvalid pause time (must be > 0).\n");
        exit(EXIT_FAILURE);
    }
    struct timespec pause_duration = {pause_ms_long / 1000, (pause_ms_long % 1000) * 1000000};

    const char *fifo_path = argv[4];
    int fifo_fd;
    char buffer[MAX_MSG_SIZE];

    // Register signal handler
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &action, NULL);

    set_colors();
    printf("\nProcess 2 (PID: %d) Started. Reading Integers. FIFO: %s\n", getpid(), fifo_path);
    reset_colors();
    fflush(stdout); // Ensure message is printed immediately

    // Open FIFO for writing - P5 should have created it
    fifo_fd = open(fifo_path, O_WRONLY);
    if (fifo_fd == -1) {
        set_colors();
        perror("\nProcess 2: Failed to open FIFO for writing\n");
        reset_colors();
        exit(EXIT_FAILURE);
    }

    while (!terminate_flag) {
        int value;
        set_colors();
        printf("\nProcess 2: Enter an integer: ");
        reset_colors();
        fflush(stdout);

        if (scanf("%d", &value) == 1) {
            // Clear rest of the input line
            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            snprintf(buffer, sizeof(buffer), "2: %d", value);

            // Add newline for P5 file format consistency
            strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);

            if (write(fifo_fd, buffer, strlen(buffer)) == -1) {
                if (errno == EPIPE) {
                    set_colors();
                    fprintf(stderr, "\nProcess 2: FIFO connection closed by P5. Terminating.\n");
                    reset_colors();
                    terminate_flag = 1; // Stop the loop
                } else {
                    set_colors();
                    perror("\nProcess 2: Failed to write to FIFO\n");
                    reset_colors();
                    // Decide whether to continue or terminate on other errors
                }
            } else {
                // Pause only after successful send
                nanosleep(&pause_duration, NULL);
            }
        } else {
            // Handle invalid input
            set_colors();
            fprintf(stderr, "\nProcess 2: Invalid input. Please enter an integer.\n");
            reset_colors();
            // Clear invalid input from buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            if (feof(stdin) || ferror(stdin)) { // Check for EOF or stream error
                set_colors();
                fprintf(stderr, "\nProcess 2: Input stream error or EOF reached. Terminating.\n");
                reset_colors();
                terminate_flag = 1;
            }
            // Don't pause after invalid input, prompt again quickly
        }
        fflush(stdout); // Ensure prompts/errors are seen
    }

    // Cleanup
    close(fifo_fd);
    set_colors();
    printf("\nProcess 2 (PID: %d) Finishing.\n", getpid());
    reset_colors();
    fflush(stdout);

    return 0;
}