/**
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * 
 */

#include "interrupts.hpp"

// Global vectors (MUST be outside main)
std::vector<Partition> partition_table;
std::vector<PCB> pcb_table;
std::vector<ExternalFile> external_files;


/**
 * Initialize the system with partitions and init process
 * Called at the start of simulation before processing trace file
 */
void initialize_system() {
    // Create 6 fixed partitions
    partition_table = {
        {1, 40, "free"},
        {2, 25, "free"},
        {3, 15, "free"},
        {4, 10, "free"},
        {5, 8, "free"},
        {6, 2, "init"}
    };
    
    // Create init process (PID 0) that uses partition 6
    PCB init_process;
    init_process.pid = 0;
    init_process.program_name = "init";
    init_process.partition_number = 6;
    init_process.size = 2;
    init_process.state = "running";
    pcb_table.push_back(init_process);
}


/**
 * Handle FORK system call
 * 
 * Steps:
 * a. Execute SYSCALL interrupt (vector 2)
 * b. Clone parent PCB to create child
 * c. Call scheduler
 * d. Return from ISR
 */
std::string handle_fork(int& current_time, std::vector<std::string>& vectors) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    // Step 1: SYSCALL interrupt boilerplate
    // Includes: switch to kernel, save context, find vector, load address
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 2, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Step 2: Clone parent PCB to create child
    PCB parent = pcb_table[0];  // Parent is first process in table
    PCB child = parent;          // Copy parent's PCB
    child.pid = pcb_table.size(); // New child gets next PID
    pcb_table.push_back(child);   // Add child to process table
    
    result += std::to_string(current_time) + ", 1, PCB cloned for child process\n";
    current_time += 1;
    
    // Step 3: Return from ISR
    result += execute_iret(current_time);
    
    // Step 4: Restore context and switch to user mode
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}


/**
 * Handle EXEC system call
 * 
 * Steps:
 * e. Execute SYSCALL interrupt (vector 3)
 * f. Search for program in external files
 * g. Find empty partition where executable fits
 * h. Simulate loader (15ms per MB of program size)
 * i. Mark partition as occupied
 * j. Update PCB with new program information
 * k. Call scheduler
 * l. Return from ISR
 */
std::string handle_exec(const std::string& program_name, int& current_time,
                        std::vector<std::string>& vectors, 
                        std::vector<ExternalFile>& external_files) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    // Step e: SYSCALL interrupt boilerplate (vector 3 for EXEC)
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 3, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Step f: Search for program in external files
    int program_size = 0;
    bool found = false;
    
    for (auto& file : external_files) {
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
    
    // Step g: Find empty partition where executable fits
    int partition_to_use = -1;
    for (auto& part : partition_table) {
        if (part.code == "free" && part.size >= program_size) {
            partition_to_use = part.number;
            part.code = program_name;  // Mark as occupied
            break;
        }
    }
    
    if (partition_to_use == -1) {
        result += std::to_string(current_time) + ", 1, ERROR: No partition available\n";
        current_time += 1;
        result += execute_iret(current_time);
        result += restore_context(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    // Step h: Simulate loader execution
    // Duration = 15 milliseconds per MB of program size (NOT random)
    int loader_time = program_size * 15;
    result += std::to_string(current_time) + ", " + std::to_string(loader_time)
            + ", loading " + program_name + " from disk to partition "
            + std::to_string(partition_to_use) + "\n";
    current_time += loader_time;
    
    // Step i & j: Mark partition as occupied and update PCB
    // Update parent process (PID 0) with new program information
    pcb_table[0].program_name = program_name;
    pcb_table[0].partition_number = partition_to_use;
    pcb_table[0].size = program_size;
    
    result += std::to_string(current_time) + ", 1, PCB updated with new program info\n";
    current_time += 1;
    
    // Step k: Return from ISR
    result += execute_iret(current_time);
    
    // Step l: Restore context and switch to user mode
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}


/**
 * Simulate CPU execution for a given duration
 */
std::string simulate_cpu(int duration, int& current_time) {
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(duration) + ", "
                       + "CPU execution\n";
    current_time += duration;
    return result;
}

/**
 * Execute ISR (Interrupt Service Routine)
 * Handles the actual device/interrupt processing
 */
std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type) {
    int isr_delay = delays[device_num];
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(isr_delay) + ", "
                       + isr_type + ": run the ISR\n";
    current_time += isr_delay;
    return result;
}

/**
 * Execute IRET (Return from Interrupt)
 * Returns to interrupted code with mode bit = 1
 */
std::string execute_iret(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, IRET\n";
    current_time += 1;
    return result;
}

/**
 * Restore CPU context (10ms operation)
 * Restores registers and processor state
 */
std::string restore_context(int& current_time) {
    const int CONTEXT_TIME = 10;
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(CONTEXT_TIME) + ", "
                       + "context restored\n";
    current_time += CONTEXT_TIME;
    return result;
}

/**
 * Switch to user mode (mode bit = 1)
 */
std::string switch_to_user_mode(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, switch to user mode\n";
    current_time += 1;
    return result;
}

/**
 * Main interrupt handling wrapper
 * Combines boilerplate, ISR execution, and return to user mode
 */
std::string handle_interrupt(int device_num, int& current_time, 
                            std::vector<std::string>& vectors,
                            std::vector<int>& delays, 
                            const std::string& interrupt_type) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    // Interrupt boilerplate: switch to kernel, save context, load vector
    auto [boilerplate, new_time] = intr_boilerplate(current_time, device_num, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Execute the ISR for this device
    result += execute_isr(device_num, current_time, delays, interrupt_type);
    
    // Return from interrupt
    result += execute_iret(current_time);
    
    // Restore context and return to user mode
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

/**
 * Generate interrupt boilerplate sequence
 * 
 * Sequence:
 * 1. Switch to kernel mode (1ms)
 * 2. Save context (context_save_time ms, typically 10ms)
 * 3. Find interrupt vector in memory
 * 4. Load ISR address into PC
 */
std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, 
                                              int context_save_time, 
                                              std::vector<std::string> vectors) {
    std::string execution = "";
    
    // Step 1: Switch to kernel mode (mode bit = 0)
    execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
    current_time++;
    
    // Step 2: Save context
    execution += std::to_string(current_time) + ", " + std::to_string(context_save_time) 
              + ", context saved\n";
    current_time += context_save_time;
    
    // Step 3: Find vector in memory
    // Vector address = ADDR_BASE + (intr_num * VECTOR_SIZE)
    char vector_address_c[10];
    sprintf(vector_address_c, "0x%04X", (ADDR_BASE + (intr_num * VECTOR_SIZE)));
    std::string vector_address(vector_address_c);
    
    execution += std::to_string(current_time) + ", 1, find vector " + std::to_string(intr_num)
              + " in memory position " + vector_address + "\n";
    current_time++;
    
    // Step 4: Load ISR address into PC
    execution += std::to_string(current_time) + ", 1, load address " + vectors.at(intr_num) 
              + " into the PC\n";
    current_time++;
    
    return std::make_pair(execution, current_time);
}

/**
 * Split string by delimiter
 * Used for parsing comma-separated values
 */
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

/**
 * Parse trace file entry
 * Format: "activity,value"
 * 
 * Examples:
 *   "CPU,50" -> ("CPU", 50)
 *   "SYSCALL,3" -> ("SYSCALL", 3)
 *   "FORK," -> ("FORK", -1)
 *   "EXEC program1," -> ("EXEC program1", -1)
 */
std::tuple<std::string, int> parse_trace(std::string trace) {
    auto parts = split_delim(trace, ",");
    if (parts.size() < 2) {
        std::cerr << "Error: Malformed input line: " << trace << std::endl;
        return {"null", -1};
    }
    
    auto activity = parts[0];
    int value;
    
    try {
        value = std::stoi(parts[1]);
    } catch (...) {
        value = -1;
    }
    
    return {activity, value};
}

/**
 * Parse command line arguments
 * 
 * Usage: ./interrupts <trace_file> <vector_table> <device_delays>
 * 
 * Arguments:
 *   argv[1]: Trace file with CPU, FORK, EXEC, SYSCALL, END_IO operations
 *   argv[2]: Vector table file with ISR addresses
 *   argv[3]: Device delays file with ISR duration for each device
 */
std::tuple<std::vector<std::string>, std::vector<int>> parse_args(int argc, char** argv) {
    if(argc != 4) {
        std::cout << "ERROR!\nExpected 3 arguments, received " << argc - 1 << std::endl;
        std::cout << "Usage: ./interrupts <trace_file> <vector_table> <device_delays>\n";
        exit(1);
    }
    
    // Validate trace file
    std::ifstream input_file;
    input_file.open(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << argv[1] << std::endl;
        exit(1);
    }
    input_file.close();
    
    // Load vector table
    std::ifstream input_vector_table;
    input_vector_table.open(argv[2]);
    if (!input_vector_table.is_open()) {
        std::cerr << "Error: Unable to open file: " << argv[2] << std::endl;
        exit(1);
    }
    
    std::string vector;
    std::vector<std::string> vectors;
    while(std::getline(input_vector_table, vector)) {
        vectors.push_back(vector);
    }
    input_vector_table.close();
    
    // Load device delays
    std::string duration;
    std::vector<int> delays;
    std::ifstream device_table;
    device_table.open(argv[3]);
    if (!device_table.is_open()) {
        std::cerr << "Error: Unable to open file: " << argv[3] << std::endl;
        exit(1);
    }
    
    while(std::getline(device_table, duration)) {
        delays.push_back(std::stoi(duration));
    }
    device_table.close();
    
    return {vectors, delays};
}

/**
 * Write execution trace to output file
 * Creates "execution.txt" with complete simulation results
 */
void write_output(std::string execution) {
    std::ofstream output_file("execution.txt");
    if (output_file.is_open()) {
        output_file << execution;
        output_file.close();
        std::cout << "File content generated successfully." << std::endl;
    } else {
        std::cerr << "Error opening file!" << std::endl;
    }
    std::cout << "Output generated in execution.txt" << std::endl;
}

/**
 * Main simulation loop
 * 
 * 1. Parse command line arguments
 * 2. Initialize system (partitions and init PCB)
 * 3. Process trace file entries:
 *    - CPU: simulate CPU execution
 *    - FORK: clone parent process
 *    - EXEC: load new program
 *    - SYSCALL/END_IO: handle interrupts
 * 4. Output final system state
 */
int main(int argc, char** argv) {
    // Parse command line arguments
    auto [vectors, delays] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);
    std::string trace;
    std::string execution;
    int current_time = 0;
    
    // Initialize system BEFORE processing trace file
    initialize_system();
    
    // Process each entry in trace file
    while(std::getline(input_file, trace)) {
        auto [activity, duration_intr] = parse_trace(trace);
        
        if(activity == "CPU") {
            // Simulate CPU execution for given duration
            execution += simulate_cpu(duration_intr, current_time);
        }
        else if (activity == "FORK") {
            // Fork creates child process
            execution += handle_fork(current_time, vectors);
        }
        else if (activity.substr(0, 4) == "EXEC") {
            // Extract program name: "EXEC program_name" -> program_name
            std::string program_name = activity.substr(5);
            execution += handle_exec(program_name, current_time, vectors, external_files);
        }
        else if (activity == "SYSCALL" || activity == "END_IO"){
            // Handle general interrupt
            execution += handle_interrupt(duration_intr, current_time, vectors, delays, activity);
        }
    }
    
    input_file.close();
    
    // Output final system state
    execution += "\n\n=== FINAL SYSTEM STATE ===\n";
    execution += "Partition Table:\n";
    for (auto& part : partition_table) {
        execution += "Partition " + std::to_string(part.number) + ": "
                  + std::to_string(part.size) + " MB - Code: " + part.code + "\n";
    }
    
    execution += "\nPCB Table:\n";
    for (auto& pcb : pcb_table) {
        execution += "PID " + std::to_string(pcb.pid) + ": " + pcb.program_name
                  + " (Partition " + std::to_string(pcb.partition_number) + ", "
                  + std::to_string(pcb.size) + " MB, State: " + pcb.state + ")\n";
    }
    
    // Write results to file
    write_output(execution);
    
    return 0;
}
