# tests/Makefile - Host-side unit tests for MiniRTOS
#
# Usage:
#   make          - Build and run all tests
#   make test     - Same as above
#   make clean    - Remove build artifacts
#   make coverage - Build with coverage, run, generate report

CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -std=c11
CFLAGS += -DTEST_HOST_BUILD
CFLAGS += -I../config -I../common -I../include -I../kernel -I../port
CFLAGS += -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

# Test source files
TEST_SRC = test_framework.c mock_port.c \
           test_heap4.c test_task.c test_scheduler.c \
           test_queue.c test_semaphore.c test_mutex.c \
           test_timer.c test_eventgroup.c test_mempool.c \
           test_mailbox.c test_notify.c test_watchdog.c \
           test_main.c

# Kernel source files (the code under test)
KERNEL_SRC = ../kernel/heap4.c ../kernel/task.c ../kernel/scheduler.c \
             ../kernel/kernel.c ../kernel/queue.c ../kernel/semaphore.c \
             ../kernel/mutex.c ../kernel/timer.c ../kernel/eventgroup.c \
             ../kernel/mempool.c ../kernel/mailbox.c ../kernel/notify.c \
             ../kernel/watchdog.c

# Platform-specific flags
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Windows)
    LDFLAGS =
    EXT = .exe
else
    LDFLAGS = -lm
    EXT =
endif

.PHONY: test clean coverage

test: test_runner$(EXT)
	./test_runner$(EXT)

test_runner$(EXT): $(TEST_SRC) $(KERNEL_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

coverage: CFLAGS += --coverage
coverage: LDFLAGS += --coverage
coverage: test
	@echo "Coverage data generated."
	@if command -v gcovr >/dev/null 2>&1; then \
		mkdir -p coverage; \
		gcovr -r .. --html -o coverage/index.html --filter '../kernel/.*'; \
		echo "Report: tests/coverage/index.html"; \
	else \
		echo "Install gcovr for HTML reports: pip install gcovr"; \
	fi

clean:
	rm -f test_runner$(EXT) *.o *.gcda *.gcno *.gcov
	rm -rf coverage
