/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"

// Global vectors (MUST be outside main)
std::vector<Partition> partition_table;
std::vector<PCB> pcb_table;
std::vector<ExternalFile> external_files;

// Initialize the system
void initialize_system() {
    partition_table = {
        {1, 40, "free"},
        {2, 25, "free"},
        {3, 15, "free"},
        {4, 10, "free"},
        {5, 8, "free"},
        {6, 2, "init"}
    };
    
    PCB init_process;
    init_process.pid = 0;
    init_process.program_name = "init";
    init_process.partition_number = 6;
    init_process.size = 2;
    init_process.state = "running";
    pcb_table.push_back(init_process);
}

// Handle FORK
std::string handle_fork(int& current_time, std::vector<std::string>& vectors) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 2, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Clone parent PCB
    PCB parent = pcb_table[0];
    PCB child = parent;
    child.pid = pcb_table.size();
    pcb_table.push_back(child);
    
    result += std::to_string(current_time) + ", 1, PCB cloned for child process\n";
    current_time += 1;
    
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

// Handle EXEC
std::string handle_exec(const std::string& program_name, int& current_time, 
                        std::vector<std::string>& vectors, std::vector<ExternalFile>& external_files) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 3, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Find program size
    int program_size = 0;
    for (auto& file : external_files) {
        if (file.program_name == program_name) {
            program_size = file.size;
            break;
        }
    }
    
    // Find empty partition
    int partition_to_use = -1;
    for (auto& part : partition_table) {
        if (part.code == "free" && part.size >= program_size) {
            partition_to_use = part.number;
            part.code = program_name;
            break;
        }
    }
    
    if (partition_to_use == -1) {
        result += std::to_string(current_time) + ", 1, ERROR: No partition available\n";
        current_time += 1;
    } else {
        int loader_time = program_size * 15;
        result += std::to_string(current_time) + ", " + std::to_string(loader_time) 
                  + ", loading " + program_name + "\n";
        current_time += loader_time;
        
        pcb_table[0].program_name = program_name;
        pcb_table[0].partition_number = partition_to_use;
        pcb_table[0].size = program_size;
    }
    
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

int main(int argc, char** argv) {
    auto [vectors, delays] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    std::string trace;
    std::string execution;
    int current_time = 0;

    // INITIALIZE SYSTEM FIRST (before loop)
    initialize_system();

    // Parse trace file
    while(std::getline(input_file, trace)) {
        auto [activity, duration_intr] = parse_trace(trace);

        if(activity == "CPU") {
            execution += simulate_cpu(duration_intr, current_time);
        } 
        else if (activity == "FORK") {
            execution += handle_fork(current_time, vectors);
        }
        else if (activity.substr(0, 4) == "EXEC") {
            std::string program_name = activity.substr(5);
            execution += handle_exec(program_name, current_time, vectors, external_files);
        }
        else if (activity == "SYSCALL" || activity == "END_IO"){
            execution += handle_interrupt(duration_intr, current_time, vectors, delays, activity);
        }
    }

    input_file.close();

    // Output final state
    execution += "\n\n=== FINAL SYSTEM STATE ===\n";
    execution += "Partition Table:\n";
    for (auto& part : partition_table) {
        execution += "Partition " + std::to_string(part.number) + ": " 
                    + std::to_string(part.size) + " MB - Code: " + part.code + "\n";
    }

    write_output(execution);

    return 0;
}

/*
* Function to simulate CPU time 
*/

std::string simulate_cpu(int duration, int& current_time) {

    std::string result = std::to_string(current_time) + ", " 
                        + std::to_string(duration) + ", "
                        + "CPU execution\n";
    current_time += duration;
    return result;

}

/*
    ISR execution function, simulates the execution of an ISR for a given device
    device_num: the device number (index in the delays vector)
    current_time: reference to the current time in the simulation
    delays: vector of delays for each device

    returns a string representing the ISR execution log
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

/*
    IRET execution function, simulates the execution of the IRET instruction
    current_time: reference to the current time in the simulation

    returns a string representing the IRET execution log
*/
std::string execute_iret(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, IRET\n";
    current_time += 1;
    return result;
}

/*
    restore_context function, simulates the restoration of the CPU context
    current_time: reference to the current time in the simulation

    returns a string representing the context restoration log
*/
std::string restore_context(int& current_time) {
    const int CONTEXT_TIME = 10;
    std::string result = std::to_string(current_time) + ", " 
                        + std::to_string(CONTEXT_TIME) + ", " 
                        + "context restored\n";
    current_time += CONTEXT_TIME;
    return result;
}

/*
    switch_to_user_mode function, simulates switching the CPU back to user mode
    current_time: reference to the current time in the simulation

    returns a string representing the switch to user mode log
*/
std::string switch_to_user_mode(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, switch to user mode\n";
    current_time += 1;
    return result;
}


/*
    handle_interrupt function, simulates the entire interrupt handling process
    device_num: the device number (index in the vectors and delays vectors)
    current_time: reference to the current time in the simulation
    vectors: vector of ISR addresses for each device
    delays: vector of delays for each device
    interrupt_type: string representing the type of interrupt (e.g., "SYSCALL", "END_IO")

    returns a string representing the complete interrupt handling log
*/
std::string handle_interrupt(int device_num, int& current_time, std::vector<std::string>& vectors, std::vector<int>& delays, const std::string& interrupt_type) {

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
