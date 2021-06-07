Tiny Shell Lab

Computer Systems - CSCI 2400

August 2018

Description:
Using C++ build a tiny shell that handles user input and responds accordingly. Some functions included: Execute built in command line commands like "quit" and display background/foreground jobs ("bg","fg", and "jobs"), fork child processes, and signal handlers (sigchild) for terminating child processes and dealing with zombie children.


Files:
Makefile	# Compiles your shell program and runs the tests
README		# This file
tsh.c		# The shell program that you will write and hand in
jobs.c		# routines to manipulate a 'jobs' data structure
helper-routines	# routines that you will use, but do not need to write
tshref		# The reference shell binary.

# The remaining files are used to test your shell
sdriver.pl	# The trace-driven shell driver
trace*.txt	# The 15 trace files that control the shell driver
tshref.out 	# Example output of the reference shell on all 15 traces

# Little C programs that are called by the trace files
myspin.c	# Takes argument <n> and spins for <n> seconds
mysplit.c	# Forks a child that spins for <n> seconds
mystop.c        # Spins for <n> seconds and sends SIGTSTP to itself
myint.c         # Spins for <n> seconds and sends SIGINT to itself