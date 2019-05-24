/**
 * virtmem.c
 * Written by Michael Ballantyne
 * Modified by Didem Unat
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TLB_SIZE 16
#define PAGES 64
#define PAGE_MASK 255

#define PAGE_SIZE 256
#define OFFSET_BITS 8
#define OFFSET_MASK 255

#define MEMORY_SIZE 256 * PAGE_SIZE

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10

struct tlbentry {
  unsigned char logical;
  unsigned char physical;
};

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];
int lru_tracker[PAGES];//this is to track when each page is accessed
int counter =0; //initially zero, increases each time there is an access and used to keep track of lru info
int fifo_ptr = 0;
signed char main_memory[MEMORY_SIZE];

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}
int min(int a, int b)
{
  if (a < b)
    return a;
  return b;
}
int search_table(int ad){

  for(int i = 0; i<PAGES; i++){
    if(pagetable[i] ==  ad)
      return i;
  }

  return -1;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(unsigned char logical_page) {
  int i;
  for (i = max((tlbindex - TLB_SIZE), 0); i < tlbindex; i++) {
    struct tlbentry *entry = &tlb[i % TLB_SIZE];

    if (entry->logical == logical_page) {
      return entry->physical;
    }
  }

  return -1;
}

/* Adds the specified mapping to the TLB, replacing the oldest mapping (FIFO replacement). */
void add_to_tlb(unsigned char logical, unsigned char physical) {
  struct tlbentry *entry = &tlb[tlbindex % TLB_SIZE];
  tlbindex++;
  entry->logical = logical;
  entry->physical = physical;
}

int main(int argc, const char *argv[])
{
  if (argc != 5) {
    fprintf(stderr, "Usage ./virtmem backingstore input\n");
    exit(1);
  }

  const char *backing_filename = argv[1];
  int backing_fd = open(backing_filename, O_RDONLY);
  backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0);

  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");

  // Fill page table entries with -1 for initially empty table.
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;
    lru_tracker[i]=-1;
  }

  // Character buffer for reading lines of input file.
  char buffer[BUFFER_SIZE];

  // Data we need to keep track of to compute stats at end.
  int total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;

  // Number of the next unallocated physical page in main memory
  unsigned char free_page = 0;

  int dir = atoi(argv[4]); //dir =0 fifo =1 lru
  while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
    total_addresses++;
    counter++;
    int logical_address = atoi(buffer);
    int offset = logical_address & OFFSET_MASK;
    int logical_page = (logical_address >> OFFSET_BITS) & PAGE_MASK;

    int physical_page = search_tlb(logical_page);
    // TLB hit
    if (physical_page != -1) {
      tlb_hits++;
      // TLB miss
    } else {
      physical_page = search_table(logical_page);
      // Page fault
      if (physical_page == -1) {
	page_faults++;

	physical_page = search_table(-1);
  if (physical_page == -1){ //free slot not found

    if(dir == 0){

      physical_page=(fifo_ptr%PAGES);

      fifo_ptr++;
    }else if(dir == 1){
      int comp = 100000000;

      for(int i =0; i<PAGES; i++){
        comp=min(comp, lru_tracker[i]);
        if (comp==lru_tracker[i]){ // if the slot is indeed min
          physical_page=i;
        }
      }

    }else{
      printf("Error in direction\n" );
    }

  }


  // printf("Free page is %d\n",free_page );
  printf("Came this far\n");


	// Copy page from backing file into physical memory
	memcpy(main_memory + physical_page * PAGE_SIZE, backing + logical_page * PAGE_SIZE, PAGE_SIZE);

    	pagetable[physical_page] = logical_page;      }

      add_to_tlb(logical_page, physical_page);
    }


    lru_tracker[physical_page]=counter;
    int physical_address = (physical_page << OFFSET_BITS) | offset;
    signed char value = main_memory[physical_page * PAGE_SIZE + offset];

    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
  }

  printf("Number of Translated Addresses = %d\n", total_addresses);
  printf("Page Faults = %d\n", page_faults);
  printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
  printf("TLB Hits = %d\n", tlb_hits);
  printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));

  return 0;
}
