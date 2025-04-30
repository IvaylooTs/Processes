#define _POSIX_C_SOURCE 200809L // For nanosleep
#include "common.h" // Може да съдържа FIFO_PATH, MQ_NAME, SOCKET_PATH, цветови кодове и др.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

// --- Global state for running processes ---
pid_t pid_p2 = 0;
pid_t pid_p3 = 0;
pid_t pid_p4 = 0;
pid_t pid_p5 = 0;
int running_children_count = 0;

// --- Global vars for log file name and common params (to be set dynamically) ---
char *log_filename_arg = NULL;      // For P5 log file
char common_text_color[3];      // To store common text color after input
char common_bg_color[3];        // To store common background color after input
char common_pause_ms_str[11];   // To store common pause time as string after input
int common_params_set = 0;      // Flag: 0 = not set yet, 1 = set

// --- Function to get common parameters from user input ---
void get_common_child_params() {
    // Use stderr for prompts here to separate from P1 Menu prompt
    fprintf(stderr,"--- Setting Common Parameters for P2, P3, P4 ---\n");

    // --- Prompt for Text Color with 4 examples ---
    fprintf(stderr,"Enter COMMON text color code (e.g., 31=Red, 32=Green, 34=Blue, 97=Bright White): ");
    fflush(stderr); // Ensure prompt is visible before scanf blocks
    if (scanf("%2s", common_text_color) != 1) { // Store directly in common variable
        fprintf(stderr, "\n[P1 Error]: Failed to read text color. Using default.\n");
        strncpy(common_text_color, "37", sizeof(common_text_color) -1); // Default white
        common_text_color[sizeof(common_text_color)-1] = '\0';
        // Clear buffer after error
        int c; while ((c = getchar()) != '\n' && c != EOF);
    } else {
        // Clear buffer after successful read
        int c; while ((c = getchar()) != '\n' && c != EOF);
    }


    // --- Prompt for Background Color with 4 examples ---
    fprintf(stderr,"Enter COMMON background color code (e.g., 40=Black, 41=Red, 44=Blue, 47=White): ");
    fflush(stderr);
    if (scanf("%2s", common_bg_color) != 1) { // Store directly in common variable
        fprintf(stderr, "\n[P1 Error]: Failed to read background color. Using default.\n");
        strncpy(common_bg_color, "40", sizeof(common_bg_color) -1); // Default black
        common_bg_color[sizeof(common_bg_color)-1] = '\0';
        int c; while ((c = getchar()) != '\n' && c != EOF);
    } else {
        int c; while ((c = getchar()) != '\n' && c != EOF);
    }


    fprintf(stderr,"Enter COMMON pause time in milliseconds (e.g., 500): ");
    fflush(stderr);
    if (scanf("%10s", common_pause_ms_str) != 1) { // Store directly in common variable
        fprintf(stderr, "\n[P1 Error]: Failed to read pause time. Using default (1000ms).\n");
        strncpy(common_pause_ms_str, "1000", sizeof(common_pause_ms_str) -1);
        common_pause_ms_str[sizeof(common_pause_ms_str)-1] = '\0';
        int c; while ((c = getchar()) != '\n' && c != EOF);
    } else {
        // Basic validation for pause time (optional but good)
        long pause_val = strtol(common_pause_ms_str, NULL, 10);
        if (pause_val <= 0) {
            fprintf(stderr, "[P1 Warning]: Invalid pause time entered (%ldms). Using 10000ms instead.\n", pause_val);
            strncpy(common_pause_ms_str, "10000", sizeof(common_pause_ms_str) -1);
            common_pause_ms_str[sizeof(common_pause_ms_str)-1] = '\0';
        }
        int c; while ((c = getchar()) != '\n' && c != EOF);
    }


    common_params_set = 1; // Mark parameters as set
    fprintf(stderr,"Common parameters set (Text:%s, Bg:%s, Pause:%sms).\n",
            common_text_color, common_bg_color, common_pause_ms_str);
    fprintf(stderr,"----------------------------------------------\n");
    fflush(stderr);
}


// --- Function to start Process 5 (uses log_filename_arg) ---
void ensure_p5_running() {
    if (pid_p5 == 0) {
        if (log_filename_arg == NULL) {
            fprintf(stderr, "[P1 Error]: Log filename argument missing. Cannot start P5.\n");
            fflush(stderr);
            return;
        }
        fprintf(stderr,"[P1 Info]: Starting Process 5 (Logger), Log file: %s\n", log_filename_arg);
        fflush(stderr);
        pid_p5 = fork();
        if (pid_p5 == -1) {
            perror("[P1 Error]: Failed to fork Process 5");
        } else if (pid_p5 == 0) {
            execlp("./process5", "process5", log_filename_arg, (char *)NULL);
            perror("[P1 Error]: Failed to exec Process 5");
            exit(EXIT_FAILURE);
        } else {
            fprintf(stderr,"[P1 Info]: Process 5 started with PID: %d\n", pid_p5);
            fflush(stderr);
            struct timespec ts = {0, 100 * 1000000};
            nanosleep(&ts, NULL);
        }
    }
}

// --- Function to stop_process (uses stderr) ---
void stop_process(pid_t *pid_ptr, int process_num) {
    if (*pid_ptr > 0) {
        fprintf(stderr, "\n[P1 Info]: Stopping Process %d (PID: %d)...\n", process_num, *pid_ptr);
        fflush(stderr);
        if (kill(*pid_ptr, SIGTERM) == -1) {
            if (errno == ESRCH) {
                fprintf(stderr, "[P1 Warning]: Process %d (PID: %d) already terminated.\n", process_num, *pid_ptr);
            } else {
                perror("[P1 Error]: Failed to send SIGTERM");
            }
        } else {
            int status;
            if (waitpid(*pid_ptr, &status, 0) == -1 && errno != ECHILD) {
                perror("[P1 Warning]: waitpid error while stopping");
            } else {
                fprintf(stderr, "[P1 Info]: Process %d (PID: %d) confirmed terminated.\n", process_num, *pid_ptr);
            }
            fflush(stderr);
        }
        *pid_ptr = 0;
        running_children_count--;

        if (running_children_count == 0 && pid_p5 > 0) {
            fprintf(stderr,"[P1 Info]: All children (P2, P3, P4) stopped. Stopping Process 5 (PID: %d)...\n", pid_p5);
            fflush(stderr);
            if (kill(pid_p5, SIGTERM) == -1) {
                if (errno == ESRCH) {
                    fprintf(stderr, "[P1 Warning]: Process 5 (PID: %d) already terminated.\n", pid_p5);
                } else {
                    perror("[P1 Error]: Failed to send SIGTERM to Process 5");
                }
            } else {
                int status;
                if(waitpid(pid_p5, &status, 0) == -1 && errno != ECHILD) {
                    perror("[P1 Warning]: waitpid error while stopping P5");
                } else {
                    fprintf(stderr,"[P1 Info]: Process 5 (PID: %d) confirmed terminated.\n", pid_p5);
                }
                fflush(stderr);
            }
            pid_p5 = 0;
        }
    } else {
        fprintf(stderr,"[P1 Info]: Process %d is not running.\n", process_num);
        fflush(stderr);
    }
}


// --- Main function ---
int main(int argc, char *argv[]) {

    // --- Check for log file name argument ONLY ---
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <log_filename>\n", argv[0]);
        fprintf(stderr, "Example: %s my_system_log.txt\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    log_filename_arg = argv[1]; // Store log filename

    // Variables for menu choice
    int choice;

    fprintf(stderr,"Main Process (P1) Started. PID: %d\n", getpid());
    fprintf(stderr,"Process 5 Log File will be: %s\n", log_filename_arg);
    fprintf(stderr,"Common parameters for P2/P3/P4 will be requested on first start.\n");
    fflush(stderr);

    // Clean up IPC
    unlink(FIFO_PATH);
    mq_unlink(MQ_NAME);
    unlink(SOCKET_PATH);

    display_menu(); // Display menu once at the beginning

    while (1) {
        // Prompt on stdout
        fflush(stdout);

        // Read menu choice
        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "\n[P1 Error]: Invalid input. Please enter a number.\n");
            fflush(stderr);
            int c; while ((c = getchar()) != '\n' && c != EOF);
            display_menu(); // Show menu again on error
            continue;
        }
        int c; while ((c = getchar()) != '\n' && c != EOF); // Clear buffer

        // --- Check if common parameters need to be set before starting P2/P3/P4 ---
        if ((choice >= 1 && choice <= 3) && common_params_set == 0) {
            // Call the function to get parameters ONLY if the flag is not set
            get_common_child_params();
            // Need to show P1 prompt again after getting params
            printf("\n[P1 Menu]: ");
            fflush(stdout);
            // Reread choice, because user might have changed their mind,
            // or just hit enter after param input. Simplest is to just loop again.
            // Alternatively, assume the original choice is still valid.
            // Let's assume the original choice is valid for now.
        }

        // Process the choice
        switch (choice) {
            case 1: // Start P2
                if (pid_p2 > 0) {
                    fprintf(stderr,"[P1 Info]: Process 2 is already running (PID: %d).\n", pid_p2); fflush(stderr);
                } else if (common_params_set == 0) { // Should not happen if check above works, but safety check
                    fprintf(stderr,"[P1 Error]: Cannot start P2, common parameters not set yet (should have been prompted).\n"); fflush(stderr);
                } else { // Parameters are set, proceed
                    ensure_p5_running();
                    if (pid_p5 > 0 || running_children_count > 0) {
                        fprintf(stderr,"[P1 Info]: Starting Process 2 with common parameters...\n"); fflush(stderr);
                        pid_p2 = fork();
                        if (pid_p2 == -1) { perror("[P1 Error]: Failed to fork Process 2"); }
                        else if (pid_p2 == 0) {
                            execlp("./process2", "process2",
                                   common_text_color, common_bg_color, common_pause_ms_str,
                                   FIFO_PATH, (char *)NULL);
                            perror("[P1 Error]: Failed to exec Process 2");
                            exit(EXIT_FAILURE);
                        } else {
                            fprintf(stderr,"[P1 Info]: Process 2 started with PID: %d\n", pid_p2); fflush(stderr);
                            running_children_count++;
                        }
                    } else {
                        fprintf(stderr,"[P1 Error]: Prerequisite Process 5 is not running. Cannot start Process 2.\n"); fflush(stderr);
                    }
                }
                break;

            case 2: // Start P3
                if (pid_p3 > 0) {
                    fprintf(stderr,"[P1 Info]: Process 3 is already running (PID: %d).\n", pid_p3); fflush(stderr);
                } else if (common_params_set == 0) {
                    fprintf(stderr,"[P1 Error]: Cannot start P3, common parameters not set yet (should have been prompted).\n"); fflush(stderr);
                } else {
                    ensure_p5_running();
                    if (pid_p5 > 0 || running_children_count > 0) {
                        fprintf(stderr,"[P1 Info]: Starting Process 3 with common parameters...\n"); fflush(stderr);
                        pid_p3 = fork();
                        if (pid_p3 == -1) { perror("[P1 Error]: Failed to fork Process 3"); }
                        else if (pid_p3 == 0) {
                            execlp("./process3", "process3",
                                   common_text_color, common_bg_color, common_pause_ms_str,
                                   MQ_NAME, (char *)NULL);
                            perror("[P1 Error]: Failed to exec Process 3");
                            exit(EXIT_FAILURE);
                        } else {
                            fprintf(stderr,"[P1 Info]: Process 3 started with PID: %d\n", pid_p3); fflush(stderr);
                            running_children_count++;
                        }
                    } else {
                        fprintf(stderr,"[P1 Error]: Prerequisite Process 5 is not running. Cannot start Process 3.\n");
                        fflush(stderr);
                        display_menu();
                        fflush(stderr);
                    }
                }
                break;

            case 3: // Start P4
                if (pid_p4 > 0) {
                    fprintf(stderr,"[P1 Info]: Process 4 is already running (PID: %d).\n", pid_p4); fflush(stderr);
                } else if (common_params_set == 0) {
                    fprintf(stderr,"[P1 Error]: Cannot start P4, common parameters not set yet (should have been prompted).\n"); fflush(stderr);
                } else {
                    ensure_p5_running();
                    if (pid_p5 > 0 || running_children_count > 0) {
                        fprintf(stderr,"[P1 Info]: Starting Process 4 with common parameters...\n"); fflush(stderr);
                        pid_p4 = fork();
                        if (pid_p4 == -1) { perror("[P1 Error]: Failed to fork Process 4"); }
                        else if (pid_p4 == 0) {
                            execlp("./process4", "process4",
                                   common_text_color, common_bg_color, common_pause_ms_str,
                                   SOCKET_PATH, (char *)NULL);
                            perror("[P1 Error]: Failed to exec Process 4");
                            exit(EXIT_FAILURE);
                        } else {
                            fprintf(stderr,"[P1 Info]: Process 4 started with PID: %d\n", pid_p4); fflush(stderr);
                            running_children_count++;
                        }
                    } else {
                        fprintf(stderr,"[P1 Error]: Prerequisite Process 5 is not running. Cannot start Process 4.\n"); fflush(stderr);
                    }
                }
                break;

            case 4: // Stop P2
                stop_process(&pid_p2, 2);
                fprintf(stderr,"----------------------------------------\n");
                display_menu();
                break;
            case 5: // Stop P3
                stop_process(&pid_p3, 3);
                fprintf(stderr,"----------------------------------------\n");
                display_menu();
                break;
            case 6: // Stop P4
                stop_process(&pid_p4, 4);
                fprintf(stderr,"----------------------------------------\n");
                display_menu();
                break;

            case 7: // Exit
                if (running_children_count > 0 || pid_p5 > 0) {
                    fprintf(stderr, "[P1 Error]: Cannot exit. Stop all other processes first (P2, P3, P4).\n");
                    // ... (print running processes using stderr) ...
                    if (pid_p2 > 0) fprintf(stderr," - P2 (PID %d) is running.\n", pid_p2);
                    if (pid_p3 > 0) fprintf(stderr," - P3 (PID %d) is running.\n", pid_p3);
                    if (pid_p4 > 0) fprintf(stderr," - P4 (PID %d) is running.\n", pid_p4);
                    if (pid_p5 > 0 && running_children_count > 0) fprintf(stderr," - P5 (PID %d) is running (required by P2/P3/P4).\n", pid_p5);
                    else if (pid_p5 > 0) fprintf(stderr," - P5 (PID %d) is running (should stop soon).\n", pid_p5);
                    fflush(stderr);
                } else {
                    fprintf(stderr,"[P1 Info]: Exiting Main Process (P1).\n"); fflush(stderr);
                    unlink(FIFO_PATH);
                    mq_unlink(MQ_NAME);
                    unlink(SOCKET_PATH);
                    return 0; // Exit
                }
                break;
            default:
                fprintf(stderr, "[P1 Error]: Invalid choice (%d). Please try again.\n", choice); fflush(stderr);
                display_menu(); // Show menu on invalid choice
                break;
        }

        // --- Връщаме паузата в края на цикъла на P1 ---
        struct timespec ts = {0, 50 * 1000000}; // 50 ms пауза
        nanosleep(&ts, NULL);

    } // end while

    return 0; // Should not reach here
}