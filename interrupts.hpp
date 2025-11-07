/**
 * @file interrupts.hpp
 * @brief Header file for OS Simulator - Part 3 & 4
 * Contains structure definitions, function declarations, and global variable declarations
 * 
 * This implementation handles:
 * - CPU execution and interrupt handling (SYSCALL/END_IO)
 * - Process management (FORK/EXEC) with multi-process scheduling
 */

#ifndef INTERRUPTS_HPP_
#define INTERRUPTS_HPP_

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tuple>
#include <cstdio>
#include <queue>
#include <map>

// Simulation constants for memory and timing calculations
#define ADDR_BASE 0          // Base address for vector table memory
#define VECTOR_SIZE 2        // Bytes per vector table entry
#define CPU_SPEED 100        // CPU clock speed (reserved for future use)
#define MEM_LIMIT 1          // Memory limit per process (reserved for future use)

// Structure definitions for memory management and process control

/**
 * @struct Partition
 * @brief Represents a fixed memory partition in the system
 * 
 * The system uses 5 fixed partitions for process memory allocation.
 * Partition 5 is reserved for the init process only.
 */
struct Partition {
    unsigned int number;    // Partition identifier (0-4, reserved: 5)
    unsigned int size;      // Partition size in megabytes
    std::string code;       // Current contents: "free", "init", or program name
};

/**
 * @struct PCB
 * @brief Process Control Block - tracks all information about a process
 * 
 * Maintains complete state for each process in the system.
 * Key feature: parent-child relationships enabled by ppid field for FORK operations
 */
struct PCB {
    int pid;                // Process identifier (unique within system)
    int ppid;               // Parent process identifier (-1 if init process)
    std::string program_name; // Name of the program currently executing
    int partition_number;   // Which partition is allocated to this process
    int size;               // Program size in megabytes
    std::string state;      // Current process state: "running", "waiting", "ready", "terminated"
    int priority;           // Priority level: 0 = normal, 1 = child (executes first)
};

/**
 * @struct ExternalFile
 * @brief Represents a program stored on disk (simulated storage)
 * 
 * Used to simulate the disk storage and loading behavior during EXEC operations.
 * Loading time is calculated as: size * 15 milliseconds
 */
struct ExternalFile {
    std::string program_name; // Program identifier for lookup
    unsigned int size;        // Program size in megabytes
};

// Process management and system call functions

/**
 * @brief Initialize the operating system simulator
 * 
 * Called once at program startup to set up system state:
 * - Creates 5 fixed memory partitions (40, 25, 15, 10, 8 MB)
 * - Creates init process (PID 0) in reserved partition
 * - Resets process ID counter to 1
 * - Initializes ready queue with init process
 */
void initialize_system();

/**
 * @brief Load available programs from external file
 * 
 * Reads external_files.txt to populate the list of programs that can be
 * loaded via EXEC system call. File format is: program_name,size_in_mb
 * 
 * @param filename Path to the external files list
 * @return Vector of available ExternalFile entries
 * @note Issues warning if file cannot be opened (continues with empty list)
 */
std::vector<ExternalFile> load_external_files(const std::string& filename);

/**
 * @brief Handle FORK system call (Interrupt Vector 2)
 * 
 * Creates a child process by executing the following steps:
 * 1. Trigger interrupt (vector 2)
 * 2. Clone parent process's PCB
 * 3. Assign new unique PID to child
 * 4. Set child's parent PID and priority
 * 5. Add child process to ready queue
 * 6. Return from interrupt
 * 
 * Child process has higher priority than parent (executes first)
 * 
 * @param current_time Reference to simulation time (gets updated)
 * @param vectors Interrupt vector table addresses from vectors.txt
 * @param current_pid Process ID of the process calling FORK
 * @return Execution trace string formatted for output file
 */
std::string handle_fork(int& current_time, std::vector<std::string>& vectors, int current_pid);

/**
 * @brief Handle EXEC system call (Interrupt Vector 3)
 * 
 * Loads and executes a new program by:
 * 1. Trigger interrupt (vector 3)
 * 2. Find requested program in external files
 * 3. Allocate memory using first-fit algorithm
 * 4. Simulate disk load (duration = size * 15ms)
 * 5. Update process PCB with program information
 * 6. Call scheduler for context switch
 * 7. Return from interrupt
 * 
 * @param program_name Name of the program to load from disk
 * @param current_time Reference to simulation time (gets updated)
 * @param vectors Interrupt vector table addresses from vectors.txt
 * @param external_files List of programs available on simulated disk
 * @param current_pid Process ID of the process calling EXEC
 * @return Execution trace string formatted for output file
 */
std::string handle_exec(const std::string& program_name, int& current_time,
                        std::vector<std::string>& vectors, 
                        std::vector<ExternalFile>& external_files,
                        int current_pid);

/**
 * @brief Find available memory partition for a program
 * 
 * Uses first-fit allocation algorithm:
 * - Scans partition table sequentially from start to end
 * - Returns first partition marked "free" with sufficient size
 * - Returns -1 if no suitable partition is found
 * 
 * @param program_size Required memory size in megabytes
 * @return Partition number (0-4) if successful, -1 if allocation failed
 */
int find_available_partition(unsigned int program_size);

/**
 * @brief Write system status snapshot to output file
 * 
 * Records PCB state at a specific simulation time point.
 * Called after FORK/EXEC operations for system status tracking.
 * 
 * @param status_file Output file stream reference
 * @param current_time Current simulation time in milliseconds
 * @param trace_line Current trace line being executed
 */
void write_system_status(std::ofstream& status_file, int current_time, 
                         const std::string& trace_line);

/**
 * @brief Get next process from the ready queue for execution
 * 
 * Implements process scheduling by:
 * - Removing the front process from the ready queue
 * - Automatically prioritizing child processes over parent processes
 * 
 * @return Process ID of next process to execute, or -1 if queue is empty
 */
int get_next_process();

/**
 * @brief Add process to the ready queue
 * 
 * Called when a process is ready to execute (after FORK or at startup)
 * 
 * @param pid Process ID to add to queue
 */
void add_to_ready_queue(int pid);

/**
 * @brief Remove process from the ready queue
 * 
 * Marks process as terminated and removes from active queue
 * 
 * @param pid Process ID to remove from queue
 */
void remove_from_ready_queue(int pid);

/**
 * @brief Check if parent-child relationship exists
 * 
 * Utility function to verify if a process is the child of another process
 * 
 * @param pid Process to check
 * @param parent_pid Suspected parent process
 * @return true if pid's parent equals parent_pid, false otherwise
 */
bool is_child_of(int pid, int parent_pid);

/**
 * @brief Mark process as terminated
 * 
 * Updates the process PCB state to "terminated" for cleanup purposes
 * 
 * @param pid Process ID to terminate
 */
void terminate_process(int pid);

// CPU and interrupt simulation functions

/**
 * @brief Simulate CPU execution for specified duration
 * 
 * Logs CPU activity at current simulation time. Automatically advances
 * current_time by the specified duration amount.
 * 
 * Output format: "time, duration, CPU execution"
 * 
 * @param duration How long the CPU executes in milliseconds
 * @param current_time Reference to simulation time counter
 * @return Formatted trace string for output file
 */
std::string simulate_cpu(int duration, int& current_time);

/**
 * @brief Execute Interrupt Service Routine for a device
 * 
 * Simulates device interrupt processing. The ISR duration comes from delays.txt
 * and is indexed by device_num parameter.
 * 
 * Output format: "time, duration, SYSCALL/END_IO: run the ISR"
 * 
 * @param device_num Device or interrupt number (index into delays array)
 * @param current_time Reference to simulation time counter
 * @param delays Vector of ISR durations loaded from delays.txt
 * @param isr_type Type of ISR: either "SYSCALL" or "END_IO"
 * @return Formatted trace string for output file
 */
std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type);

/**
 * @brief Execute IRET (Return from Interrupt) instruction
 * 
 * Marks the return from interrupt handler back to the interrupted code.
 * Standard operation takes 1 millisecond.
 * 
 * Output format: "time, 1, IRET"
 * 
 * @param current_time Reference to simulation time counter
 * @return Formatted trace string for output file
 */
std::string execute_iret(int& current_time);

/**
 * @brief Restore processor context after interrupt
 * 
 * Restores CPU registers and process state to their pre-interrupt values.
 * Standard operation takes 10 milliseconds.
 * 
 * Output format: "time, 10, context restored"
 * 
 * @param current_time Reference to simulation time counter
 * @return Formatted trace string for output file
 */
std::string restore_context(int& current_time);

/**
 * @brief Switch processor from kernel mode to user mode
 * 
 * Marks the transition back to user process execution after interrupt handling.
 * Takes 1 millisecond.
 * 
 * Output format: "time, 1, switch to user mode"
 * 
 * @param current_time Reference to simulation time counter
 * @return Formatted trace string for output file
 */
std::string switch_to_user_mode(int& current_time);

/**
 * @brief Handle complete interrupt sequence
 * 
 * Combines all interrupt processing steps into a unified handler:
 * 1. Execute boilerplate (kernel switch, context save, vector lookup)
 * 2. Execute ISR with appropriate delay
 * 3. Execute IRET
 * 4. Restore context
 * 5. Return to user mode
 * 
 * @param device_num Device or interrupt number
 * @param current_time Reference to simulation time counter
 * @param vectors Interrupt vector table addresses
 * @param delays ISR durations from delays.txt
 * @param interrupt_type Type: either "SYSCALL" or "END_IO"
 * @return Complete interrupt trace string for output file
 */
std::string handle_interrupt(int device_num, int& current_time,
                            std::vector<std::string>& vectors,
                            std::vector<int>& delays,
                            const std::string& interrupt_type);

// Interrupt handling utility functions

/**
 * @brief Generate interrupt boilerplate sequence
 * 
 * Executes the standard interrupt entry sequence:
 * 1. Switch to kernel mode (1ms)
 * 2. Save context (10ms)
 * 3. Find vector in memory (1ms)
 * 4. Load ISR address into PC (1ms)
 * 
 * Total time for boilerplate: 13 milliseconds before ISR execution
 * 
 * @param current_time Current simulation time
 * @param intr_num Interrupt number (used for vector lookup)
 * @param context_save_time Context save duration (typically 10ms)
 * @param vectors Interrupt vector table loaded from vectors.txt
 * @return Pair containing (trace_string, new_time_after_boilerplate)
 */
std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, 
                                              int context_save_time, 
                                              std::vector<std::string> vectors);

// File input/output and parsing functions

/**
 * @brief Parse and validate command line arguments
 * 
 * Loads all input files required for simulation:
 * 1. trace.txt - sequence of simulation events
 * 2. vectors.txt - interrupt vector addresses
 * 3. delays.txt - ISR execution durations
 * 4. external_files.txt - available programs on disk
 * 
 * Program exits with error if any file cannot be opened or has invalid format.
 * 
 * @param argc Number of command line arguments provided
 * @param argv Array of command line argument strings
 * @return Tuple containing (vectors_array, delays_array)
 * 
 * Usage: ./interrupts trace.txt vector_table.txt device_table.txt external_files.txt
 */
std::tuple<std::vector<std::string>, std::vector<int>> parse_args(int argc, char** argv);

/**
 * @brief Parse a single line from the trace file
 * 
 * Extracts activity name and numeric value from comma-separated format
 * 
 * Examples:
 *   "CPU,50" yields ("CPU", 50)
 *   "FORK,10" yields ("FORK", 10)
 *   "EXEC program1,50" yields ("EXEC program1", 50)
 *   "IF_CHILD,0" yields ("IF_CHILD", 0)
 *   "SYSCALL,3" yields ("SYSCALL", 3)
 *   "ENDIF,0" yields ("ENDIF", 0)
 * 
 * @param trace One line from the trace file
 * @return Tuple containing (activity_string, numeric_value)
 */
std::tuple<std::string, int> parse_trace(std::string trace);

/**
 * @brief Split a string by a delimiter
 * 
 * Generic string splitting utility for parsing various input formats
 * Removes the delimiter from the resulting tokens
 * 
 * @param input String to be split
 * @param delim Delimiter character or substring
 * @return Vector of token strings
 */
std::vector<std::string> split_delim(std::string input, std::string delim);

/**
 * @brief Write complete execution trace to output file
 * 
 * Creates "execution.txt" containing:
 * - All simulation events with exact timestamps
 * - Final system state (partition table, PCB table)
 * 
 * @param execution Complete execution trace string
 */
void write_output(std::string execution);

/**
 * @brief Write system status snapshots to file
 * 
 * Creates "system_status.txt" with PCB table snapshots
 * Snapshots recorded after FORK/EXEC operations
 * 
 * @param status System status string with PCB snapshots
 */
void write_system_status_file(std::string status);

// Global variable declarations

extern std::vector<Partition> partition_table;      // Memory partition table
extern std::vector<PCB> pcb_table;                  // Process control blocks
extern std::vector<ExternalFile> external_files;   // Programs on simulated disk
extern std::queue<int> ready_queue;                 // Processes ready to execute
extern std::map<int, std::vector<int>> parent_child_map;  // Process parent-child relationships

#endif
