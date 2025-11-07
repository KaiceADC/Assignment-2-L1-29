/**
 * @file interrupts.cpp
 * Implementation of OS Simulator - Part 3 and Part 4
 * Handles CPU execution, interrupts, process management, and system calls
 */

#include "interrupts.hpp"

// Global variable definitions - these maintain system state across function calls
std::vector<Partition> partition_table;           // System memory partitions
std::vector<PCB> pcb_table;                        // All processes in system
std::vector<ExternalFile> external_files;         // Programs on disk
std::queue<int> ready_queue;                       // Processes ready to run
std::map<int, std::vector<int>> parent_child_map; // Parent-child relationships
int next_pid = 1;                                  // Counter for next process ID
int suspended_parent_pid = -1;                     // Tracks suspended parent

// System initialization - sets up memory and initial process
void initialize_system() {
    // Create 5 fixed memory partitions with different sizes
    partition_table = {
        {0, 40, "free"},
        {1, 25, "free"},
        {2, 15, "free"},
        {3, 10, "free"},
        {4, 8, "free"}
    };
    
    // Create init process that starts the system
    PCB init_process;
    init_process.pid = 0;
    init_process.ppid = -1;
    init_process.program_name = "init";
    init_process.partition_number = 5;  // Reserved for init only
    init_process.size = 0;
    init_process.state = "running";
    init_process.priority = 0;
    pcb_table.push_back(init_process);
    
    ready_queue.push(0);
    next_pid = 1;
}

// Load programs from external file - simulates disk access
std::vector<ExternalFile> load_external_files(const std::string& filename) {
    std::vector<ExternalFile> files;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open " << filename << std::endl;
        return files;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto parts = split_delim(line, ",");
        if (parts.size() == 2) {
            ExternalFile ext_file;
            ext_file.program_name = parts[0];
            ext_file.size = std::stoi(parts[1]);
            files.push_back(ext_file);
        }
    }
    file.close();
    return files;
}

// Memory management - find free partition for program
int find_available_partition(unsigned int program_size) {
    for (auto& part : partition_table) {
        // First-fit algorithm: return first free partition that fits
        if (part.code == "free" && part.size >= program_size) {
            return part.number;
        }
    }
    return -1;  // No suitable partition found
}

// Process queue management - add process for execution
void add_to_ready_queue(int pid) {
    ready_queue.push(pid);
}

// Get next process to execute from queue
int get_next_process() {
    if (ready_queue.empty()) return -1;
    int next = ready_queue.front();
    ready_queue.pop();
    return next;
}

// Check process relationships
bool is_child_of(int pid, int parent_pid) {
    for (auto& pcb : pcb_table) {
        if (pcb.pid == pid && pcb.ppid == parent_pid) {
            return true;
        }
    }
    return false;
}

// FORK system call - creates child process by cloning parent
std::string handle_fork(int& current_time, std::vector<std::string>& vectors, int current_pid) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    // Interrupt boilerplate: kernel switch + context save + vector lookup
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 2, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Find parent process to clone
    PCB parent;
    bool found = false;
    for (const auto& pcb : pcb_table) {
        if (pcb.pid == current_pid) {
            parent = pcb;
            found = true;
            break;
        }
    }
    
    if (!found) {
        result += std::to_string(current_time) + ", 1, ERROR: Parent not found\n";
        current_time += 1;
        result += execute_iret(current_time);
        result += restore_context(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    // Clone parent to create child process
    PCB child = parent;
    child.pid = next_pid++;           // Assign new process ID
    child.ppid = current_pid;         // Set parent relationship
    child.priority = 1;               // Child has higher priority
    pcb_table.push_back(child);
    parent_child_map[current_pid].push_back(child.pid);
    add_to_ready_queue(child.pid);
    
    // Log PCB cloning operation
    int fork_duration = 1;
    result += std::to_string(current_time) + ", " + std::to_string(fork_duration)
           + ", cloning the PCB\n";
    current_time += fork_duration;
    
    // Scheduler runs at no time cost
    int sched_duration = 0;
    result += std::to_string(current_time) + ", " + std::to_string(sched_duration)
           + ", scheduler called\n";
    
    // Return from interrupt
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

// EXEC system call - loads program from disk into memory
std::string handle_exec(const std::string& program_name, int& current_time,
                        std::vector<std::string>& vectors, 
                        std::vector<ExternalFile>& external_files,
                        int current_pid) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    // Interrupt boilerplate
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 3, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Search for program in external files (disk)
    int program_size = 0;
    bool found = false;
    for (const auto& file : external_files) {
        if (file.program_name == program_name) {
            program_size = file.size;
            found = true;
            break;
        }
    }
    
    if (!found) {
        result += std::to_string(current_time) + ", 1, ERROR: Program not found\n";
        current_time += 1;
        result += execute_iret(current_time);
        result += restore_context(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    // Find partition for program (first-fit algorithm)
    int partition_to_use = find_available_partition(program_size);
    
    if (partition_to_use == -1) {
        result += std::to_string(current_time) + ", 1, ERROR: No partition\n";
        current_time += 1;
        result += execute_iret(current_time);
        result += restore_context(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    // Mark partition as occupied with program
    for (auto& part : partition_table) {
        if (part.number == partition_to_use) {
            part.code = program_name;
            break;
        }
    }
    
    // Simulate disk load: time = size * 15ms per MB
    int program_mb_size = program_size;
    int loader_time = program_mb_size * 15;
    result += std::to_string(current_time) + ", " + std::to_string(loader_time)
            + ", loading " + program_name + " from disk to partition " 
            + std::to_string(partition_to_use) + "\n";
    current_time += loader_time;
    
    // Mark partition as occupied
    int mark_duration = 1;
    result += std::to_string(current_time) + ", " + std::to_string(mark_duration)
           + ", marking partition as occupied\n";
    current_time += mark_duration;
    
    // Update process control block with new program info
    int update_duration = 3;
    result += std::to_string(current_time) + ", " + std::to_string(update_duration)
           + ", updating PCB\n";
    current_time += update_duration;
    
    for (auto& pcb : pcb_table) {
        if (pcb.pid == current_pid) {
            pcb.program_name = program_name;
            pcb.partition_number = partition_to_use;
            pcb.size = program_size;
            break;
        }
    }
    
    // Scheduler runs
    int sched_duration = 0;
    result += std::to_string(current_time) + ", " + std::to_string(sched_duration)
           + ", scheduler called\n";
    
    // Return from interrupt
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

// CPU execution simulation - process runs for specified duration
std::string simulate_cpu(int duration, int& current_time) {
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(duration) + ", CPU execution\n";
    current_time += duration;
    return result;
}

// Execute interrupt service routine - runs device handler
std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type) {
    int isr_delay = delays[device_num];
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(isr_delay) + ", "
                       + isr_type + ": run the ISR\n";
    current_time += isr_delay;
    return result;
}

// IRET - return from interrupt
std::string execute_iret(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, IRET\n";
    current_time += 1;
    return result;
}

// Restore context - restore CPU state after interrupt
std::string restore_context(int& current_time) {
    const int CONTEXT_TIME = 10;
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(CONTEXT_TIME) + ", context restored\n";
    current_time += CONTEXT_TIME;
    return result;
}

// Switch to user mode - return from kernel to user execution
std::string switch_to_user_mode(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, switch to user mode\n";
    current_time += 1;
    return result;
}

// Handle interrupt - complete sequence from entry to return
std::string handle_interrupt(int device_num, int& current_time,
                            std::vector<std::string>& vectors,
                            std::vector<int>& delays,
                            const std::string& interrupt_type) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    auto [boilerplate, new_time] = intr_boilerplate(current_time, device_num, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    result += execute_isr(device_num, current_time, delays, interrupt_type);
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

// Interrupt boilerplate - standard entry sequence for all interrupts
std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, 
                                              int context_save_time, 
                                              std::vector<std::string> vectors) {
    std::string execution = "";
    
    // Step 1: Switch to kernel mode
    execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
    current_time++;
    
    // Step 2: Save processor context
    execution += std::to_string(current_time) + ", " + std::to_string(context_save_time) 
              + ", context saved\n";
    current_time += context_save_time;
    
    // Step 3: Calculate vector table address
    char vector_address_c[10];
    sprintf(vector_address_c, "0x%04X", (ADDR_BASE + (intr_num * VECTOR_SIZE)));
    std::string vector_address(vector_address_c);
    
    // Step 4: Find vector in memory
    execution += std::to_string(current_time) + ", 1, find vector " + std::to_string(intr_num)
              + " in memory position " + vector_address + "\n";
    current_time++;
    
    // Step 5: Load ISR address into program counter
    execution += std::to_string(current_time) + ", 1, load address " + vectors.at(intr_num) 
              + " into the PC\n";
    current_time++;
    
    return std::make_pair(execution, current_time);
}

// String parsing utility - split by delimiter
std::vector<std::string> split_delim(std::string input, std::string delim) {
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    std::string token;
    
    while ((pos = input.find(delim)) != std::string::npos) {
        token = input.substr(0, pos);
        tokens.push_back(token);
        input.erase(0, pos + delim.length());
    }
    tokens.push_back(input);
    return tokens;
}

// Parse trace file line
std::tuple<std::string, int> parse_trace(std::string trace) {
    auto parts = split_delim(trace, ",");
    if (parts.size() < 2) {
        return {"null", -1};
    }
    
    auto activity = parts[0];
    int value = -1;
    
    try {
        value = std::stoi(parts[1]);
    } catch (...) {
        value = -1;
    }
    
    return {activity, value};
}

// Parse command line arguments and load input files
std::tuple<std::vector<std::string>, std::vector<int>> parse_args(int argc, char** argv) {
    if(argc != 5) {
        std::cout << "ERROR! Expected 4 arguments\n";
        std::cout << "Usage: ./interrupts <trace> <vectors> <delays> <external_files>\n";
        exit(1);
    }
    
    // Verify trace file exists
    std::ifstream input_file(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open " << argv[1] << std::endl;
        exit(1);
    }
    input_file.close();
    
    // Load interrupt vector table
    std::ifstream input_vector_table(argv[2]);
    if (!input_vector_table.is_open()) {
        std::cerr << "Error: Unable to open " << argv[2] << std::endl;
        exit(1);
    }
    
    std::string vector;
    std::vector<std::string> vectors;
    while(std::getline(input_vector_table, vector)) {
        vectors.push_back(vector);
    }
    input_vector_table.close();
    
    // Load device delay table
    std::string duration;
    std::vector<int> delays;
    std::ifstream device_table(argv[3]);
    if (!device_table.is_open()) {
        std::cerr << "Error: Unable to open " << argv[3] << std::endl;
        exit(1);
    }
    
    while(std::getline(device_table, duration)) {
        delays.push_back(std::stoi(duration));
    }
    device_table.close();
    
    return {vectors, delays};
}

// Write execution trace to output file
void write_output(std::string execution) {
    std::ofstream output_file("execution.txt");
    if (output_file.is_open()) {
        output_file << execution;
        output_file.close();
        std::cout << "File content generated successfully." << std::endl;
    } else {
        std::cerr << "Error opening file!" << std::endl;
    }
}

// Write system status snapshots
void write_system_status_file(std::string status) {
    std::ofstream status_file("system_status.txt");
    if (status_file.is_open()) {
        status_file << status;
        status_file.close();
    }
}

// Main simulation loop - processes trace file and generates output
int main(int argc, char** argv) {
    auto [vectors, delays] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);
    std::string trace;
    std::string execution;
    std::string status;
    int current_time = 0;
    int current_pid = 0;
    bool in_child_block = false;
    bool in_parent_block = false;
    int block_pid = -1;
    
    initialize_system();
    external_files = load_external_files(argv[4]);
    
    // Process each trace line
    while(std::getline(input_file, trace)) {
        auto [activity, value] = parse_trace(trace);
        
        // Simulate CPU execution
        if(activity == "CPU") {
            execution += simulate_cpu(value, current_time);
        }
        // Handle FORK system call
        else if (activity == "FORK") {
            execution += handle_fork(current_time, vectors, current_pid);
        }
        // Mark start of child-only code
        else if (activity == "IF_CHILD") {
            in_child_block = true;
            in_parent_block = false;
            block_pid = value;
        }
        // Mark start of parent-only code
        else if (activity == "IF_PARENT") {
            in_child_block = false;
            in_parent_block = true;
            block_pid = value;
        }
        // Mark end of conditional block
        else if (activity == "ENDIF") {
            in_child_block = false;
            in_parent_block = false;
            block_pid = -1;
        }
        // Handle EXEC system call
        else if (activity.substr(0, 4) == "EXEC") {
            std::string program_name = activity.substr(5);
            execution += handle_exec(program_name, current_time, vectors, external_files, current_pid);
        }
        // Handle device interrupts
        else if (activity == "SYSCALL" || activity == "END_IO"){
            execution += handle_interrupt(value, current_time, vectors, delays, activity);
        }
    }
    
    input_file.close();
    
    // Add final system state to output
    execution += "\nFinal System State\n";
    execution += "Partition Table:\n";
    for (const auto& part : partition_table) {
        execution += "Partition " + std::to_string(part.number) + ": "
                  + std::to_string(part.size) + " MB - Code: " + part.code + "\n";
    }
    
    execution += "\nPCB Table:\n";
    for (const auto& pcb : pcb_table) {
        execution += "PID " + std::to_string(pcb.pid);
        if (pcb.ppid != -1) {
            execution += " (Parent: " + std::to_string(pcb.ppid) + ")";
        }
        execution += ": " + pcb.program_name + " (Partition " + std::to_string(pcb.partition_number)
                  + ", " + std::to_string(pcb.size) + " MB, State: " + pcb.state + ")\n";
    }
    
    write_output(execution);
    write_system_status_file(status);
    
    return 0;
}
