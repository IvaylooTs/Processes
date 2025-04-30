CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g # Use c11 or gnu11 as needed
LDFLAGS = -lrt # Link with real-time library for message queues

TARGETS = process1 process2 process3 process4 process5

.PHONY: all clean

all: $(TARGETS)

process1: process1.c common.h
	$(CC) $(CFLAGS) process1.c -o process1

process2: process2.c common.h
	$(CC) $(CFLAGS) process2.c -o process2

process3: process3.c common.h
	$(CC) $(CFLAGS) process3.c -o process3 $(LDFLAGS)

process4: process4.c common.h
	$(CC) $(CFLAGS) process4.c -o process4

process5: process5.c common.h
	$(CC) $(CFLAGS) process5.c -o process5 $(LDFLAGS)

clean:
	rm -f $(TARGETS) $(LOG_FILE) /tmp/proc2_fifo /tmp/proc4_socket
	# Note: mq_unlink is needed to remove message queues, not just rm
	# You might need to run process1 with option 7 or manually clean MQs if needed
	# Example manual clean: ./process1, choose 7 (if it calls unlink/mq_unlink)
	# Or handle mq_unlink inside process5's cleanup more robustly.