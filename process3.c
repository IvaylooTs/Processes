#define _POSIX_C_SOURCE 200809L // For nanosleep
#include "common.h"
#include <signal.h>
#include <time.h>
#include <mqueue.h>

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
        fprintf(stderr, "\nUsage: %s <text_color_code> <bg_color_code> <pause_ms> <mq_name>\n", argv[0]);
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

    const char *mq_name = argv[4];
    mqd_t mq_desc;
    char buffer[MAX_MSG_SIZE];

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &action, NULL);

    set_colors();
    printf("\nProcess 3 (PID: %d) Started. Reading Floats. MQ: %s\n", getpid(), mq_name);
    reset_colors();
    fflush(stdout);

    // Open Message Queue for writing - P5 should have created it
    mq_desc = mq_open(mq_name, O_WRONLY);
    if (mq_desc == (mqd_t)-1) {
        set_colors();
        perror("\nProcess 3: Failed to open message queue\n");
        reset_colors();
        exit(EXIT_FAILURE);
    }

    while (!terminate_flag) {
        double value;
        set_colors();
        printf("\nProcess 3: Enter a float: ");
        reset_colors();
        fflush(stdout);

        if (scanf("%lf", &value) == 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            snprintf(buffer, sizeof(buffer), "3: %lf", value);
            // No need for newline here, P5 adds it from the message format

            if (mq_send(mq_desc, buffer, strlen(buffer) + 1, 0) == -1) { // Send null terminator too? Check P5's receive
                set_colors();
                perror("\nProcess 3: Failed to send message\n");
                reset_colors();
                // Check if queue is full (errno == EAGAIN if non-blocking, but we are blocking)
                // or if queue was closed. Assume closed on error for simplicity.
                terminate_flag = 1;
            } else {
                nanosleep(&pause_duration, NULL);
            }
        } else {
            set_colors();
            fprintf(stderr, "\nProcess 3: Invalid input. Please enter a float.\n");
            reset_colors();
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            if (feof(stdin) || ferror(stdin)) {
                set_colors();
                fprintf(stderr, "\nProcess 3: Input stream error or EOF reached. Terminating.\n");
                reset_colors();
                terminate_flag = 1;
            }
        }
        fflush(stdout);
    }

    // Cleanup
    mq_close(mq_desc);
    set_colors();
    printf("\nProcess 3 (PID: %d) Finishing.\n", getpid());
    reset_colors();
    fflush(stdout);

    return 0;
}