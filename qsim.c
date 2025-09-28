#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h> // For standard deviation, linking with -lm required in makefile

// --- Symbolic Constants [cite: 26] ---
#define MAX_TELLER_IDLE_SECONDS_SHORT 150.0 
#define MAX_TELLER_IDLE_SECONDS_LONG 600.0 // Initial idle time [cite: 25]

// --- Global Simulation Parameters ---
int NUM_CUSTOMERS = 0;
int NUM_TELLERS = 0;
float SIM_TIME_MINUTES = 0.0;
float AVG_SERVICE_TIME_MINUTES = 0.0;

// --- Simulated Clock ---
float CURRENT_TIME = 0.0; // Current time in minutes

// --- Enums and Typedefs ---
typedef enum {
    ARRIVAL,      // Customer arriving [cite: 39]
    SERVICE_END,  // Customer service completed [cite: 41, 46]
    TELLER_ACTION // Teller completed service/idle time [cite: 43, 47]
} EventType;

// Forward declarations of core structures
struct Customer;
struct Teller;
struct Event;

// --- Function Pointer Type for Event Action [cite: 53, 54] ---
typedef void (*ActionFunction)(struct Event*);

// --- 1. Customer Structure [cite: 23] ---
typedef struct Customer {
    int id;
    float arrival_time;
    float start_service_time;
    float departure_time;
    struct Customer *next; // For teller queue linked list
} Customer;

// --- 2. Teller Queue Structure [cite: 56, 57] ---
typedef struct TellerQueue {
    int id;
    Customer *head;
    Customer *tail;
    int length;
} TellerQueue;

// Global array of teller queues
TellerQueue *TellerLines = NULL;

// --- 3. Event Structure [cite: 27, 49, 50] ---
typedef struct Event {
    EventType type;
    float time; // Time of the event [cite: 29]
    ActionFunction action; // Function pointer to the action to invoke [cite: 53, 54]
    void *actor_data; // Pointer to a Customer or Teller structure
    struct Event *next; // For event queue linked list
} Event;

// Global Event Queue [cite: 27, 60]
Event *EventQueueHead = NULL;


// --- Function Prototypes for Actions (Called via Function Pointer) ---
void handle_customer_arrival(Event *e);
void handle_service_end(Event *e);
void handle_teller_action(Event *e);

// --- Linked List Helper Function Prototypes (e.g., for EventQueue) ---
void insert_event_sorted(Event *newEvent); // [cite: 61, 62]
Event* remove_event_head(); // [cite: 33, 63]

// --- Utility Functions ---
// Generates a random time in [0, SIM_TIME_MINUTES] [cite: 75, 76]
float generate_arrival_time() {
    return SIM_TIME_MINUTES * rand() / (float)RAND_MAX;
}

// Generates a random service time [cite: 78, 79]
float generate_service_time() {
    return 2.0 * AVG_SERVICE_TIME_MINUTES * rand() / (float)RAND_MAX;
}

// Selects the shortest teller line [cite: 14, 40]
TellerQueue* get_shortest_line() {
    TellerQueue *shortest = &TellerLines[0];
    int shortest_len = shortest->length;

    for (int i = 1; i < NUM_TELLERS; i++) {
        if (TellerLines[i].length < shortest_len) {
            shortest = &TellerLines[i];
            shortest_len = shortest->length;
        } else if (TellerLines[i].length == shortest_len) {
            // Randomly choose among equally short lines [cite: 14, 40]
            if (rand() % 2 == 0) {
                shortest = &TellerLines[i];
            }
        }
    }
    return shortest;
}

// --- Event Action Implementations [cite: 55] ---

// Action: New customer arrives. Add to the shortest line.
void handle_customer_arrival(Event *e) {
    printf("Function Pointer Log: Invoking handle_customer_arrival() at time %.2f\n", CURRENT_TIME); // [cite: 64]
    Customer *c = (Customer *)e->actor_data;
    TellerQueue *line = get_shortest_line(); // Logic for separate lines [cite: 39]
    
    // Add customer to the end of the selected line [cite: 39, 57]
    // ... logic to add 'c' to the linked list 'line' ...
    line->length++;
    
    // Check if the teller is idle; if so, start service immediately.
    // (A more robust solution checks teller *state*, but for simplicity,
    // we assume the teller action event will handle initiating service.)
}

// Action: Customer service is completed. Collect stats and delete event.
void handle_service_end(Event *e) {
    printf("Function Pointer Log: Invoking handle_service_end() at time %.2f\n", CURRENT_TIME); // [cite: 64]
    Customer *c = (Customer *)e->actor_data;
    c->departure_time = CURRENT_TIME;
    
    // Gather statistics: total time in bank (departure - arrival) [cite: 41, 42]
    float time_in_bank = c->departure_time - c->arrival_time;
    // ... update global statistics (total time, count, max wait, etc.) ...
    
    // Customer leaves the bank, Event object is deleted [cite: 42]
    free(c); 
    // The Event 'e' will be freed after this function returns in the main loop.
}

// Action: Teller is ready to serve next customer (or idle).
void handle_teller_action(Event *e) {
    printf("Function Pointer Log: Invoking handle_teller_action() at time %.2f\n", CURRENT_TIME); // [cite: 64]
    // Gather statistics about the teller (e.g., idle time) [cite: 43]
    // ... update global teller statistics ...

    int line_index_to_serve = -1; 
    // Logic to find a customer:
    // 1. Check if a customer is waiting in this teller's line.
    // 2. If not, check other lines and "steal" the first customer from a random one.
    
    Customer *customer_to_serve = NULL;
    // ... logic to find customer_to_serve and set line_index_to_serve ...

    if (customer_to_serve) { // Customer found [cite: 45]
        float service_time = generate_service_time();
        customer_to_serve->start_service_time = CURRENT_TIME;
        
        // 1. Create Customer Service End Event [cite: 46]
        Event *cust_end_event = (Event*)malloc(sizeof(Event));
        cust_end_event->type = SERVICE_END;
        cust_end_event->time = CURRENT_TIME + service_time;
        cust_end_event->action = handle_service_end;
        cust_end_event->actor_data = customer_to_serve;
        insert_event_sorted(cust_end_event);

        // 2. Create Teller Action Event (to look for next customer/idle) [cite: 47]
        Event *teller_next_event = (Event*)malloc(sizeof(Event));
        teller_next_event->type = TELLER_ACTION;
        teller_next_event->time = CURRENT_TIME + service_time;
        teller_next_event->action = handle_teller_action;
        // The actor_data should link back to the specific teller (not implemented here)
        teller_next_event->actor_data = e->actor_data; 
        insert_event_sorted(teller_next_event);

    } else { // No customers waiting at all [cite: 17, 44]
        float idle_time_short = 1.0 + (float)rand() / RAND_MAX * (150.0 / 60.0 - 1.0); // 1-150 seconds [cite: 44]
        
        // Put a Teller Action event back into the queue for a random idle time [cite: 44]
        Event *idle_event = (Event*)malloc(sizeof(Event));
        idle_event->type = TELLER_ACTION;
        idle_event->time = CURRENT_TIME + idle_time_short;
        idle_event->action = handle_teller_action;
        idle_event->actor_data = e->actor_data; 
        insert_event_sorted(idle_event);
        
        // ... update global teller idle time stats ...
    }
}


// --- Simulation Main Loop ---
void run_simulation(int num_customers, int num_tellers, float sim_time, float avg_service, int single_queue_mode) {
    // ... Initialization of parameters and global statistics ...
    srand(time(NULL)); // Initialize random seed
    
    // 1. Instantiate all Customer Arrival events [cite: 31, 32, 77]
    for (int i = 0; i < num_customers; i++) {
        Customer *c = (Customer*)malloc(sizeof(Customer));
        c->id = i + 1;
        c->arrival_time = generate_arrival_time();
        
        Event *e = (Event*)malloc(sizeof(Event));
        e->type = ARRIVAL;
        e->time = c->arrival_time;
        e->action = handle_customer_arrival;
        e->actor_data = c;
        insert_event_sorted(e);
    }
    
    // 2. Instantiate all Teller (initial) events [cite: 25]
    for (int i = 0; i < num_tellers; i++) {
        // ... Teller structure initialization ...
        
        Event *e = (Event*)malloc(sizeof(Event));
        e->type = TELLER_ACTION;
        e->time = 1.0 + (float)rand() / RAND_MAX * (MAX_TELLER_SECONDS_LONG / 60.0 - 1.0); // Random idle time 1-600s [cite: 25]
        e->action = handle_teller_action;
        // The actor_data should link back to the specific teller (not implemented here)
        e->actor_data = NULL; 
        insert_event_sorted(e);
    }
    
    // 3. Play out the simulation [cite: 33]
    while (EventQueueHead != NULL) {
        Event *current_event = remove_event_head(); // Take the first event [cite: 33]
        
        if (current_event->time > SIM_TIME_MINUTES) {
            // Stop if the event happens after the simulation time limit
            // ... (Need to free remaining events/customers) ...
            break;
        }

        CURRENT_TIME = current_event->time; // Advance the clock [cite: 33]
        
        // Invoke the action method associated with that event [cite: 33, 63]
        current_event->action(current_event); 
        
        free(current_event); // Event object is deleted after action [cite: 42]
    }
    
    // 4. Print out the statistics [cite: 35]
    // ... print_statistics(single_queue_mode) ...
}

int main(int argc, char *argv[]) {
    // 1. Get and interpret program parameters [cite: 22, 66]
    if (argc != 5) {
        fprintf(stderr, "Usage: %s #customers #tellers simulationTime averageServiceTime\n", argv[0]);
        return 1;
    }

    NUM_CUSTOMERS = atoi(argv[1]);
    NUM_TELLERS = atoi(argv[2]);
    SIM_TIME_MINUTES = atof(argv[3]);
    AVG_SERVICE_TIME_MINUTES = atof(argv[4]);
    
    // Convert all constants to minutes for consistency (e.g., 600s/60 = 10 min)
    
    printf("--- Simulation Parameters ---\n");
    printf("Customers: %d, Tellers: %d, Time: %.1f min, Avg Service: %.1f min\n", 
           NUM_CUSTOMERS, NUM_TELLERS, SIM_TIME_MINUTES, AVG_SERVICE_TIME_MINUTES);

    // 2. Run simulation for separate queues (as primary logic is here) [cite: 36]
    printf("\n============================================\n");
    printf("    RUNNING SIMULATION: SEPARATE QUEUES\n");
    printf("============================================\n");
    run_simulation(NUM_CUSTOMERS, NUM_TELLERS, SIM_TIME_MINUTES, AVG_SERVICE_TIME_MINUTES, 0);
    
    // 3. Run simulation for single queue [cite: 36]
    // ... Reset queues and statistics ...
    // printf("\n============================================\n");
    // printf("    RUNNING SIMULATION: SINGLE QUEUE\n");
    // printf("============================================\n");
    // run_simulation(NUM_CUSTOMERS, NUM_TELLERS, SIM_TIME_MINUTES, AVG_SERVICE_TIME_MINUTES, 1);

    return 0;
}
