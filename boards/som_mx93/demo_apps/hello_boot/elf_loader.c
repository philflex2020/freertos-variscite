
//Elf loader

//p. wilshire
//05_26_2024
//take 2
// gcc -o loader elf_loader.c

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fsl_debug_console.h"

#include "elf_loader.h"

// #define EI_NIDENT 16

// typedef struct {
//     unsigned char e_ident[EI_NIDENT];
//     uint16_t e_type;
//     uint16_t e_machine;
//     uint32_t e_version;
//     uint32_t e_entry; // Entry point address
//     uint32_t e_phoff; // Program header table file offset
//     uint32_t e_shoff; // Section header table file offset
//     uint32_t e_flags;
//     uint16_t e_ehsize;
//     uint16_t e_phentsize; // Program header table entry size
//     uint16_t e_phnum; // Program header table entry count
//     uint16_t e_shentsize;
//     uint16_t e_shnum;
//     uint16_t e_shstrndx;
// } Elf32_Ehdr;

// typedef struct {
//     uint32_t p_type;   // Type of segment
//     uint32_t p_offset; // Offset in file
//     uint32_t p_vaddr;  // Virtual address in memory
//     uint32_t p_paddr;  // Physical address (if different)
//     uint32_t p_filesz; // Size of segment in file
//     uint32_t p_memsz;  // Size of segment in memory
//     uint32_t p_flags;  // Segment attributes
//     uint32_t p_align;  // Alignment of segment
// } Elf32_Phdr;


#define ELF_MAGIC 0x7f454c46 // "\x7fELF"


void set_vtor(uint32_t address) {
    volatile uint32_t *VTOR = (uint32_t *)0xE000ED08;
    *VTOR = address;
}

//only do this for arm
void set_msp(uint32_t address) {
//    asm volatile ("msr msp, %0" : : "r" (address) : );
}

const char* get_section_type(uint32_t type) {
    switch (type) {
        case SHT_NULL: return "NULL";
        case SHT_PROGBITS: return "PROGBITS";
        case SHT_SYMTAB: return "SYMTAB";
        case SHT_STRTAB: return "STRTAB";
        case SHT_RELA: return "RELA";
        case SHT_HASH: return "HASH";
        case SHT_DYNAMIC: return "DYNAMIC";
        case SHT_NOTE: return "NOTE";
        case SHT_NOBITS: return "NOBITS";
        case SHT_REL: return "REL";
        case SHT_SHLIB: return "SHLIB";
        case SHT_DYNSYM: return "DYNSYM";
        default: return "UNKNOWN";
    }
}

void print_section_type(uint32_t type) {
    switch (type) {
        case SHT_NULL:     PRINTF("NULL     "); break;
        case SHT_PROGBITS: PRINTF("PROGBITS "); break;
        case SHT_SYMTAB:   PRINTF("SYMTAB   "); break;
        case SHT_STRTAB:   PRINTF("STRTAB   "); break;
        case SHT_RELA:     PRINTF("RELA     "); break;
        case SHT_HASH:     PRINTF("HASH     "); break;
        case SHT_DYNAMIC:  PRINTF("DYNAMIC  "); break;
        case SHT_NOTE:     PRINTF("NOTE     "); break;
        case SHT_NOBITS:   PRINTF("NOBITS   "); break;
        case SHT_REL:      PRINTF("REL      "); break;
        case SHT_SHLIB:    PRINTF("SHLIB    "); break;
        case SHT_DYNSYM:   PRINTF("DYNSYM   "); break;
        default:           PRINTF("UNKNOWN  "); break;
    }
}

void print_section_flags(uint32_t flags) {
    if (flags & SHF_WRITE)     PRINTF("W");
    if (flags & SHF_ALLOC)     PRINTF("A");
    if (flags & SHF_EXECINSTR) PRINTF("X");
}

//uint8_t *buffer = (uint8_t *)malloc(file_size);

int load_elf_file(const char *filename, unsigned char **buff, void* load_addr, int mem_size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        PRINTF("Failed to open ELF file\n");
        return -1;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *buffer = (uint8_t *)malloc(file_size);

    // Allocate a buffer to hold the entire file
    if (!buffer) {
        PRINTF("Failed to allocate memory for ELF file\n");
        fclose(file);
        return -1;
    }

    // Read the entire file into the buffer
    fread(buffer, 1, file_size, file);
    fclose(file);
    *buff = buffer;
    return 0;
}

int load_elf_buffer(char *buffer, void* load_addr, int mem_size) {

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)buffer;

    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4) != 0) {
        PRINTF("Not an ELF file\n");
        free(buffer);
        return -1;
    }

    PRINTF("Load Addr: %p\r\n", load_addr);
    PRINTF("ELF entry point: 0x%08X\r\n", (unsigned int)ehdr->e_entry);
    PRINTF("Program header offset: %u\r\n", (unsigned int)ehdr->e_phoff);
    PRINTF("Number of program headers: %u\r\n", ehdr->e_phnum);

    // Read program headers
    Elf32_Phdr *phdr = (Elf32_Phdr *)(buffer + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == 1) { // PT_LOAD
            PRINTF("Loading segment %d\r\n", i);
            PRINTF("  Offset: 0x%08X\r\n", phdr[i].p_offset);
            PRINTF("  VirtAddr: 0x%08X\r\n", phdr[i].p_vaddr);
            PRINTF("  PhysAddr: 0x%08X\r\n", phdr[i].p_paddr);
            PRINTF("  FileSize: %u\r\n", phdr[i].p_filesz);
            PRINTF("  MemSize: %u\r\n", phdr[i].p_memsz);
            PRINTF("  Flags: 0x%08X\r\n", phdr[i].p_flags);
            PRINTF("  Align: %u\r\n", phdr[i].p_align);

            // Allocate memory for segment
            void *segment = (void *)((uintptr_t)load_addr + phdr[i].p_vaddr);
            PRINTF("segment: %p\r\n", segment);

            //memcpy(segment, buffer + phdr[i].p_offset, phdr[i].p_filesz);

            // Zero out the remaining part if the memory size is larger than the file size
            if(0)
            {
                if (phdr[i].p_memsz > phdr[i].p_filesz) {
                   memset(segment + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
                }
            }

        }
    }        
    // Read section headers and section names
    Elf32_Shdr *shdr = (Elf32_Shdr *)(buffer + ehdr->e_shoff);
    char *shstrtab = (char *)(buffer + shdr[ehdr->e_shstrndx].sh_offset);

    PRINTF("Section headers:\r\n");
    for (int i = 0; i < ehdr->e_shnum; i++) {
        PRINTF("  [%2d] %s\r\n", i, shstrtab + shdr[i].sh_name);
        PRINTF("       Type: %s\r\n", get_section_type(shdr[i].sh_type));
        PRINTF("       Flags: ");
        print_section_flags(shdr[i].sh_flags);
        PRINTF("\r\n");
    }

    //
    //free(buffer);

    // Jump to entry point
    //void (*entry_point)(void) = (void (*)(void))(load_addr + ehdr->e_entry);
    //void (*entry_point)(void) = (void (*)(void))(ehdr->e_entry);
    load_addr  = 0;
    //void (*entry_point)(void) = (void (*)(void))((uintptr_t)load_addr + ehdr->e_entry);
    //entry_point();

// Set the vector table base address and stack pointer
    uint32_t *vector_table = (uint32_t *)((uintptr_t)load_addr);
    uint32_t vector_tablep = 0x550000;//(uint32_t *)((uintptr_t)load_addr);

    set_vtor((uint32_t)vector_tablep);
    set_msp(vector_table[0]);

    // Jump to the reset handler
    //void (*reset_handler)(void) = (void (*)(void))vector_table[1];
    //reset_handler();

    return 0;
}


// int main(void) {
//     // test load into temp buffer
//     //uint32_t load_address = 0x20000000; // Example load address for M33
//     int mem_size = 0x20000;
//     void* load_address = malloc(mem_size); // Example load address for M33

//     const char *elf_filename = "program.elf";

//     PRINTF("Loading ELF file...\n");
//     if (load_elf(elf_filename, load_address, mem_size) != 0) {
//         //PRINTF("Failed to load ELF file\n");
//         return -1;
//     }

//     PRINTF("ELF file loaded and running\n");
//     return 0;
// }

// ### Explanation

// 1. **ELF Header and Program Header**:
//    - The ELF header (`Elf32_Ehdr`) provides information about the file, including the entry point, the program header table offset, and the number of program headers.
//    - The program header (`Elf32_Phdr`) provides information about each segment to be loaded, including its type, offset, virtual address, physical address, file size, and memory size.

// 2. **Reading the Entire ELF File into a Buffer**:
//    - The `load_elf` function opens the ELF file, determines its size, and reads the entire file into a buffer.

// 3. **Processing the ELF File from the Buffer**:
//    - After reading the file into the buffer, the function processes the ELF header and program headers from the buffer to load the segments into memory.

// 4. **Loading Segments**:
//    - For each loadable segment (`PT_LOAD`), the function copies the segment data from the buffer to the appropriate memory location, setting up the memory as specified by the segment's file and memory sizes.

// 5. **Jumping to the Entry Point**:
//    - After loading all segments, the function jumps to the entry point specified in the ELF header to start executing the loaded program.


