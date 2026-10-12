# SimpleScheduler

> A lightweight CPU process scheduler implemented in C featuring time-sliced execution, process preemption, and configurable CPU concurrency.

SimpleScheduler is a user-space scheduling system designed to demonstrate how an operating system coordinates multiple processes competing for limited CPU resources. Paired with a custom command-line shell, it accepts executable jobs and controls their execution using a configurable number of concurrent slots and a fixed scheduling interval.

The implementation combines POSIX signals, interval timers, and inter-process communication to coordinate job execution, rotate processes through a ready queue, and collect per-job scheduling statistics.

---

## Features

### Round-Robin Process Management

Uses a circular FIFO queue to organize submitted jobs. At each scheduling interval, the scheduler selects up to `NCPU` eligible processes. Processes that remain unfinished are suspended and placed at the end of the queue, allowing other waiting jobs to receive execution time.

### Timer-Based Scheduling Loop

Uses `setitimer()` to generate periodic `SIGALRM` signals according to the configured `TSLICE`. Rather than continuously polling, the scheduler waits using `pause()` between scheduling events and performs queue management when a timer signal arrives.

### Shell–Scheduler Communication

Separates command handling from scheduling logic into two independent programs. The shell forwards submitted process IDs and executable names through a pipe, while the scheduler sends completion and waiting-time statistics back through a separate channel. Non-blocking reads allow the scheduler to check for incoming jobs without waiting indefinitely.

### Signal-Controlled Process Execution

Controls job execution through `SIGSTOP` and `SIGCONT`. Submitted children are initially suspended, resumed when selected for a CPU slot, and stopped again when their time slice ends. This provides explicit user-space control over when jobs are permitted to execute.

### Job Initialization Hook

Provides a custom `dummy_main.h` header that wraps the entry point of compatible job programs. The wrapper stops the process before calling the original job logic, allowing execution to begin only after the scheduler resumes it.

### Scheduling Statistics

Tracks scheduling intervals and execution ticks for individual jobs. When the shell exits, it displays a summary containing each submitted executable, its PID, estimated completion time, and estimated waiting time, expressed in units of the configured time quantum.

---

## Getting Started

### 1. Compilation

Compile the shell and scheduler separately using GCC:

```bash
gcc simple-shell.c -o shell
gcc SimpleScheduler.c -o SimpleScheduler
```

Ensure both executables are in the same directory, as `SimpleShell` launches `SimpleScheduler` from the current directory.

### 2. Initialization

Launch SimpleShell by specifying the number of concurrent CPU slots (`NCPU`) and the time quantum (`TSLICE`) in milliseconds.

```bash
./shell 2 100
```

This starts the shell with **2 CPU slots** and a **100 ms time quantum**.

### 3. Job Submission

Submit executable programs through the running shell using the `submit` command.

```bash
submit ./job1.out
submit ./test.out
```

Jobs are initially suspended and are resumed by the scheduler when selected for execution. Each job must be compiled as an executable and should include `dummy_main.h` to support scheduler-controlled startup.

### 4. Shutdown and Reports

When you have finished submitting jobs, enter `exit` in the shell:

```text
exit
```

The scheduler continues processing outstanding jobs before shutting down. Once scheduling is complete, SimpleShell prints a summary of submitted jobs, including their executable names, process IDs, completion times, and waiting times, measured in multiples of `TSLICE`.










































