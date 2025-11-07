#ifndef INTERRUPTS_HPP_
#define INTERRUPTS_HPP_

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tuple>
#include <cstdio>

#define ADDR_BASE 0
#define VECTOR_SIZE 2
#define CPU_SPEED 100
#define MEM_LIMIT 1

// ============================================================
// STRUCTURES FOR MEMORY MANAGEMENT AND PROCESS CONTROL
// ============================================================

// Partition structure for memory management
struct Partition {
    unsigned int number;        // Partition number (1-6)
    unsigned int size;          // Size in MB
    std::string code;           // "free", "init", or program name
};

// PCB (Process Control Block)
struct PCB {
    int pid;                    // Process ID
    std::string program_name;   // Name of the program
    int partition_number;       // Which partition it's in
    int size;                   // Size in MB
    std::string state;          // "running" or "waiting"
};

// External file (simulated disk/storage)
struct ExternalFile {
    std::string program_name;   // Program name (max 20 chars)
    unsigned int size;          // Size in MB
};

// ============================================================
// PART 4: FORK AND EXEC SYSTEM CALLS
// ============================================================

/**
 * \brief Initialize the system with partitions and init process
 * Sets up 6 fixed partitions and creates the initial PCB for init
 */
void initialize_system();

/**
 * \brief Handle FORK system call
 * Creates a child process by cloning the parent's PCB
 * 
 * @param current_time reference to current simulation time
 * @param vectors interrupt vector table addresses
 * @return string containing execution trace
 */
std::string handle_fork(int& current_time, std::vector<std::string>& vectors);

/**
 * \brief Handle EXEC system call
 * Loads and executes a new program from disk into memory
 * 
 * @param program_name name of program to load
 * @param current_time reference to current simulation time
 * @param vectors interrupt vector table addresses
 * @param external_files list of available programs on disk
 * @return string containing execution trace
 */
std::string handle_exec(const std::string& program_name, int& current_time,
                        std::vector<std::string>& vectors, 
                        std::vector<ExternalFile>& external_files);


// CPU and interrupt simulation

/**
 * \brief Simulate CPU execution
 * Logs CPU time usage
 * 
 * @param duration how long CPU executes (in simulation time)
 * @param current_time reference to current simulation time
 * @return string containing CPU execution trace
 */
std::string simulate_cpu(int duration, int& current_time);

/**
 * \brief Execute ISR (Interrupt Service Routine)
 * Simulates the execution of an ISR for a given device
 * 
 * @param device_num device/interrupt number
 * @param current_time reference to current simulation time
 * @param delays vector of device delays
 * @param isr_type type of ISR ("SYSCALL" or "END_IO")
 * @return string containing ISR execution trace
 */
std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type);

/**
 * \brief Execute IRET (Return from Interrupt)
 * Returns control from ISR back to interrupted code
 * 
 * @param current_time reference to current simulation time
 * @return string containing IRET trace
 */
std::string execute_iret(int& current_time);

/**
 * \brief Restore processor context
 * Restores saved registers and state (10ms operation)
 * 
 * @param current_time reference to current simulation time
 * @return string containing context restore trace
 */
std::string restore_context(int& current_time);

/**
 * \brief Switch to user mode
 * Transitions processor from kernel mode back to user mode
 * 
 * @param current_time reference to current simulation time
 * @return string containing mode switch trace
 */
std::string switch_to_user_mode(int& current_time);

/**
 * \brief Handle interrupt (wrapper function)
 * Combines all steps: boilerplate, ISR execution, return to user mode
 * 
 * @param device_num device/interrupt number
 * @param current_time reference to current simulation time
 * @param vectors interrupt vector table addresses
 * @param delays vector of device delays
 * @param interrupt_type type of interrupt ("SYSCALL" or "END_IO")
 * @return string containing complete interrupt trace
 */
std::string handle_interrupt(int device_num, int& current_time,
                            std::vector<std::string>& vectors,
                            std::vector<int>& delays,
                            const std::string& interrupt_type);


// Interrupt handling utilities


/**
 * \brief Generate interrupt boilerplate
 * Standard sequence: switch to kernel, save context, lookup vector, load address
 * 
 * @param current_time current simulation time
 * @param intr_num interrupt number (for vector lookup)
 * @param context_save_time time to save context (typically 10ms)
 * @param vectors interrupt vector table addresses
 * @return pair of (execution string, new time after boilerplate)
 */
std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, 
                                              int context_save_time, 
                                              std::vector<std::string> vectors);


// File I?O Parsing utilities

/**
 * \brief Parse command line arguments
 * Validates and loads trace file, vector table, and device delays
 * 
 * @param argc number of command line arguments
 * @param argv command line arguments array
 * @return tuple of (vectors, delays) loaded from files
 * 
 * Usage: ./interrupts <trace_file> <vector_table> <device_delays>
 */
std::tuple<std::vector<std::string>, std::vector<int>> parse_args(int argc, char** argv);

/**
 * \brief Parse trace file entry
 * Extracts activity name and duration/interrupt number
 * 
 * @param trace one line from trace file (format: "activity,value")
 * @return tuple of (activity, duration_or_interrupt_num)
 * 
 * Examples:
 *   "CPU,50" -> ("CPU", 50)
 *   "SYSCALL,3" -> ("SYSCALL", 3)
 *   "FORK," -> ("FORK", -1)
 */
std::tuple<std::string, int> parse_trace(std::string trace);

/**
 * \brief Split string by delimiter
 * Helper function for parsing comma-separated and space-separated values
 * 
 * @param input string to split
 * @param delim delimiter character(s)
 * @return vector of tokens
 * 
 * Examples:
 *   split_delim("a,b,c", ",") -> ["a", "b", "c"]
 *   split_delim("hello world", " ") -> ["hello", "world"]
 */
std::vector<std::string> split_delim(std::string input, std::string delim);

/**
 * \brief Write execution trace to output file
 * Creates "execution.txt" with complete simulation results
 * 
 * @param execution the complete execution trace string
 */
void write_output(std::string execution);

#endif
