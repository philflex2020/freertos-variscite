#ifndef BMSSYSTEM_RTOS_H
#define BMSSYSTEM_RTOS_H

#include <stdint.h>
// #include <sys/mman.h>
// #include <sys/stat.h>
// #include <fcntl.h>

#define RACK_COUNT 10
#define MODULE_COUNT 10
#define CELL_COUNT 40  // Example cell count, adjust as needed
#define TEMP_COUNT 5   // Example temperature count, adjust as needed
#define SHM_NAME "/my_shared_memory"


// Bms Status
typedef struct {
    uint16_t state;
    uint16_t soh;
    uint16_t soc;
    uint16_t flags; // contatcor open etc
    uint16_t charge_capacity;
    uint16_t discharge_capacity;
    uint16_t max_charge_current;
    uint16_t max_discharge_current;

    uint16_t max_rack_voltage;
    uint16_t max_rack_voltage_num;
    uint16_t max_rack_voltage_cell;
    uint16_t min_rack_voltage;
    uint16_t min_rack_voltage_num;
    uint16_t min_rack_voltage_cell;
    uint16_t max_rack_current;
    uint16_t max_rack_current_num;
    uint16_t min_rack_current;
    uint16_t min_rack_current_num;
    uint16_t max_rack_temp;
    uint16_t max_rack_temp_num;
    uint16_t max_rack_temp_unit;
    uint16_t min_rack_temp;
    uint16_t min_rack_temp_num;
    uint16_t min_rack_temp_unit;

    
} BmsStatus;

// Rack Status
typedef struct {
    uint16_t state;
    uint16_t soh;
    uint16_t soc;
    uint16_t flags; // contatcor open etc
    uint16_t charge_capacity;
    uint16_t discharge_capacity;
    uint16_t max_charge_current;
    uint16_t max_discharge_current;

    uint16_t max_module_voltage;
    uint16_t max_module_voltage_num;
    uint16_t max_module_voltage_cell;
    uint16_t min_module_voltage;
    uint16_t min_module_voltage_num;
    uint16_t min_module_voltage_cell;
    uint16_t max_module_current;
    uint16_t max_module_current_num;
    uint16_t min_module_current;
    uint16_t min_module_current_num;
    uint16_t max_module_temp;
    uint16_t max_module_temp_num;
    uint16_t max_module_temp_unit;
    uint16_t min_module_temp;
    uint16_t min_module_temp_num;
    uint16_t min_module_temp_unit;
    int rack_count;
    int data_id;
    
} RackStatus;

// Module Status
typedef struct {
    uint16_t state;
    uint16_t soh;
    uint16_t soc;
    uint16_t charge_capacity;
    uint16_t discharge_capacity;
    uint16_t max_cell_voltage;
    uint16_t max_cell_voltage_num;
    uint16_t min_cell_voltage;
    uint16_t min_cell_voltage_num;
    uint16_t max_module_current;
    uint16_t min_module_current;
    uint16_t max_cell_temp;
    uint16_t max_cell_temp_num;
    uint16_t min_cell_temp;
    uint16_t min_cell_temp_num;
    int cell_count;// = CELL_COUNT;
    int temp_count;// = TEMP_COUNT;
} ModuleStatus;

// Data structure for cell data
typedef struct {
    uint16_t voltage;
    uint16_t soc;
    uint16_t soh;
} CellData;

// Data structure for temperature data
typedef struct {
    uint16_t temperature;
} TempData;

// Data structure for Module payload
typedef struct {
    int seq_id;
    unsigned int message_id;
    int rack_id;
    int module_id;
    int cell_id;
    int cell_count;
    int temp_count;
    ModuleStatus status;
    CellData cells[CELL_COUNT];
    TempData temps[TEMP_COUNT];
} ModulePayload;

typedef struct {
    int seq_id;
    unsigned int message_id;
    int rack_id;
    int module_count;
    RackStatus status;
    ModulePayload modules[MODULE_COUNT];
} RackPayload;

typedef struct {
    int seq_id;
    int rack_count;
    BmsStatus status;
    RackPayload racks[RACK_COUNT];
} BmsData;

#define SHM_SIZE sizeof(BmsData)
#define BUFF_ADDR 0x87d80000


// void *get_shm(int *shm_fdp)
// {
//     int shm_fd;
//     char *shared_mem;
//     int new_shm  = 0;
//     shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
//     if (shm_fd == -1) {
//         perror("shm_open");
//         new_shm = 1;
//         //exit(EXIT_FAILURE);
//     }
//     if (new_shm)
//     {
//         shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
//         if (shm_fd == -1) {
//             perror("shm_open");
//             exit(EXIT_FAILURE);
//         }
//     }
//     *shm_fdp = shm_fd;
//     // Configure the size of the shared memory object
//     if (ftruncate(shm_fd, SHM_SIZE) == -1) {
//         perror("ftruncate");
//         exit(EXIT_FAILURE);
//     }

//     // Map the shared memory object
//     shared_mem = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
//     if (shared_mem == MAP_FAILED) {
//         perror("mmap");
//         exit(EXIT_FAILURE);
//     }
//     if(new_shm)
//     {
//         // Clear shared memory
//         memset(shared_mem, 0, SHM_SIZE);
//     }
//     return shared_mem;
// }

#endif
