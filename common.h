//
// Created by User on 4/14/2025.
//

#ifndef PROCESSES_COMMON_H
#define PROCESSES_COMMON_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h> // For nanosleep/usleep

void display_menu() {
    fprintf(stderr, "\n--- Main Menu ---\n");
    fprintf(stderr, "1. Start Process 2 (Integer Input)\n");
    fprintf(stderr, "2. Start Process 3 (Float Input)\n");
    fprintf(stderr, "3. Start Process 4 (String Input)\n");
    fprintf(stderr, "4. Stop Process 2\n");
    fprintf(stderr, "5. Stop Process 3\n");
    fprintf(stderr, "6. Stop Process 4\n");
    fprintf(stderr, "7. Exit Program\n\n");
    fprintf(stderr, "Enter option: ");
    fflush(stderr);
}

// --- Configuration ---
#define FIFO_PATH "/tmp/proc2_fifo"
#define MQ_NAME "/proc3_queue"
#define SOCKET_PATH "/tmp/proc4_socket"
#define MAX_MSG_SIZE 256 // Max size for message queue and buffers
#define MQ_MAX_MSGS 10    // Max messages in queue

// --- ANSI Color Codes ---
#define COL_RESET   "\x1B[0m"
#define COL_BLACK   "\x1B[30m"
#define COL_RED     "\x1B[31m"
#define COL_GREEN   "\x1B[32m"
#define COL_YELLOW  "\x1B[33m"
#define COL_BLUE    "\x1B[34m"
#define COL_MAGENTA "\x1B[35m"
#define COL_CYAN    "\x1B[36m"
#define COL_WHITE   "\x1B[37m"

#define BG_BLACK   "\x1B[40m"
#define BG_RED     "\x1B[41m"
#define BG_GREEN   "\x1B[42m"
#define BG_YELLOW  "\x1B[43m"
#define BG_BLUE    "\x1B[44m"
#define BG_MAGENTA "\x1B[45m"
#define BG_CYAN    "\x1B[46m"
#define BG_WHITE   "\x1B[47m"

// --- Helper Function (Example) ---
// You might put common helper functions here, or declare them extern
// For simplicity, the color setting function will be duplicated in P2, P3, P4.
#endif //PROCESSES_COMMON_H
