#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MAX_TELLER_IDLE_SECONDS_SHORT 150.0
#define MAX_TELLER_IDLE_MINUTES_SHORT (MAX_TELLER_IDLE_SECONDS_SHORT / 60.0)
#define MAX_TELLER_IDLE_SECONDS_LONG 600.0
#define MAX_TELLER_IDLE_MINUTES_LONG (MAX_TELLER_IDLE_SECONDS_LONG / 60.0)

int NUM_CUSTOMERS = 0;
int NUM_TELLERS = 0;
float SIM_TIME_MINUTES = 0.0;
float AVG_SERVICE_TIME_MINUTES = 0.0;

float CURRENT_TIME = 0.0;

int customers_served = 0;
float total_wait_time = 0.0;
float total_service_time = 0.0;
float total_time_in_bank = 0.0;
int single_queue_mode_flag = 0;

typedef enum {
    ARRIVAL,
    SERVICE_END,
    TELLER_ACTION
} EventType;

struct Customer;
struct Teller;
struct Event;

typedef void (*ActionFunction)(struct Event*);

typedef struct Customer {
    int id;
    float arrival_time;
    float start_service_time;
    float departure_time;
    struct Customer *next;
} Customer;

typedef struct Teller {
    int id;
} Teller;

typedef struct TellerQueue {
    int id;
    Customer *head;
    Customer *tail;
    int length;
} TellerQueue;

TellerQueue *TellerLines = NULL;
Teller *Tellers = NULL;

typedef struct Event {
    EventType type;
    float time;
    ActionFunction action;
    void *actor_data;
    struct Event *next;
} Event;

Event *EventQueueHead = NULL;

void handle_customer_arrival(Event *e);
void handle_service_end(Event *e);
void handle_teller_action(Event *e);

void insert_event_sorted(Event *newEvent);
Event* remove_event_head();
void free_all_remaining_events();

void enqueue_customer(TellerQueue *line, Customer *c);
Customer* dequeue_customer(TellerQueue *line);
Customer* steal_customer(TellerQueue *except_line);
void free_all_teller_queues();

float generate_arrival_time() {
    return SIM_TIME_MINUTES * rand() / (float)RAND_MAX;
}

float generate_service_time() {
    return 2.0 * AVG_SERVICE_TIME_MINUTES * rand() / (float)RAND_MAX;
}

TellerQueue* get_shortest_line() {
    if (NUM_TELLERS == 0) return NULL;
    TellerQueue *shortest = &TellerLines[0];
    int shortest_len = shortest->length;

    for (int i = 1; i < NUM_TELLERS; i++) {
        if (TellerLines[i].length < shortest_len) {
            shortest = &TellerLines[i];
            shortest_len = shortest->length;
        } else if (TellerLines[i].length == shortest_len) {
            if (rand() % 2 == 0) {
                shortest = &TellerLines[i];
            }
        }
    }
    return shortest;
}

void insert_event_sorted(Event *newEvent) {
    if (EventQueueHead == NULL || newEvent->time < EventQueueHead->time) {
        newEvent->next = EventQueueHead;
        EventQueueHead = newEvent;
        return;
    }

    Event *current = EventQueueHead;
    while (current->next != NULL && current->next->time <= newEvent->time) {
        current = current->next;
    }
    
    newEvent->next = current->next;
    current->next = newEvent;
}

Event* remove_event_head() {
    Event *head = EventQueueHead;
    if (head != NULL) {
        EventQueueHead = EventQueueHead->next;
        head->next = NULL;
    }
    return head;
}

void free_all_remaining_events() {
    Event *current = EventQueueHead;
    while (current != NULL) {
        Event *temp = current;
        current = current->next;
        free(temp);
    }
    EventQueueHead = NULL;
}

void enqueue_customer(TellerQueue *line, Customer *c) {
    c->next = NULL;
    if (line->head == NULL) {
        line->head = c;
        line->tail = c;
    } else {
        line->tail->next = c;
        line->tail = c;
    }
    line->length++;
}

Customer* dequeue_customer(TellerQueue *line) {
    Customer *c = line->head;
    if (c != NULL) {
        line->head = line->head->next;
        if (line->head == NULL) {
            line->tail = NULL;
        }
        c->next = NULL;
        line->length--;
    }
    return c;
}

Customer* steal_customer(TellerQueue *except_line) {
    if (single_queue_mode_flag) return NULL;

    for (int i = 0; i < NUM_TELLERS; i++) {
        TellerQueue *current_line = &TellerLines[i];
        if (current_line != except_line && current_line->length > 0) {
            return dequeue_customer(current_line);
        }
    }
    return NULL;
}

void free_all_teller_queues() {
    for (int i = 0; i < NUM_TELLERS; i++) {
        TellerQueue *line = &TellerLines[i];
        Customer *current = line->head;
        while (current != NULL) {
            Customer *temp = current;
            current = current->next;
            free(temp);
        }
        line->head = NULL;
        line->tail = NULL;
        line->length = 0;
    }
    if (TellerLines != NULL) {
        free(TellerLines);
        TellerLines = NULL;
    }
}

void handle_customer_arrival(Event *e) {
    printf("FP Log: Arrival C%d at %.2f\n", ((Customer *)e->actor_data)->id, CURRENT_TIME);
    Customer *c = (Customer *)e->actor_data;
    
    TellerQueue *line = single_queue_mode_flag ? &TellerLines[0] : get_shortest_line();
    
    enqueue_customer(line, c);
}

void handle_service_end(Event *e) {
    Customer *c = (Customer *)e->actor_data;
    c->departure_time = CURRENT_TIME;
    
    float wait_time = c->start_service_time - c->arrival_time;
    float service_time = c->departure_time - c->start_service_time;
    float time_in_bank = c->departure_time - c->start_service_time;
    
    total_wait_time += wait_time;
    total_service_time += service_time;
    total_time_in_bank += time_in_bank;
    customers_served++;
    
    printf("FP Log: Service End C%d at %.2f (Wait: %.2f, Total: %.2f)\n", 
           c->id, CURRENT_TIME, wait_time, time_in_bank);

    free(c);
}

void handle_teller_action(Event *e) {
    Teller *t = (Teller *)e->actor_data;
    printf("FP Log: Teller %d Action at %.2f\n", t->id, CURRENT_TIME);

    Customer *customer_to_serve = NULL;
    int line_index_to_serve = t->id - 1;

    TellerQueue *my_line = single_queue_mode_flag ? &TellerLines[0] : &TellerLines[line_index_to_serve];
    customer_to_serve = dequeue_customer(my_line);

    if (customer_to_serve == NULL && !single_queue_mode_flag) {
        customer_to_serve = steal_customer(my_line);
        if (customer_to_serve) {
            printf("FP Log: Teller %d STOLE C%d\n", t->id, customer_to_serve->id);
        }
    }

    if (customer_to_serve) {
        float service_time = generate_service_time();
        customer_to_serve->start_service_time = CURRENT_TIME;
        
        Event *cust_end_event = (Event*)malloc(sizeof(Event));
        cust_end_event->type = SERVICE_END;
        cust_end_event->time = CURRENT_TIME + service_time;
        cust_end_event->action = handle_service_end;
        cust_end_event->actor_data = customer_to_serve;
        insert_event_sorted(cust_end_event);

        Event *teller_next_event = (Event*)malloc(sizeof(Event));
        teller_next_event->type = TELLER_ACTION;
        teller_next_event->time = CURRENT_TIME + service_time;
        teller_next_event->action = handle_teller_action;
        teller_next_event->actor_data = t;
        insert_event_sorted(teller_next_event);

    } else {
        float idle_time_minutes = (1.0 / 60.0) + (float)rand() / RAND_MAX * (MAX_TELLER_IDLE_MINUTES_SHORT - (1.0 / 60.0));
        
        Event *idle_event = (Event*)malloc(sizeof(Event));
        idle_event->type = TELLER_ACTION;
        idle_event->time = CURRENT_TIME + idle_time_minutes;
        idle_event->action = handle_teller_action;
        idle_event->actor_data = t;
        insert_event_sorted(idle_event);
        
        printf("FP Log: Teller %d goes IDLE until %.2f\n", t->id, idle_event->time);
    }
}

void print_statistics(int single_queue_mode) {
    if (customers_served == 0) {
        printf("\nNo customers were served during the simulation time.\n");
        return;
    }
    
    printf("\n--- Simulation Results (%s) ---\n", single_queue_mode ? "Single Queue" : "Separate Queues");
    printf("Customers Served: %d\n", customers_served);
    printf("Simulation End Time: %.2f minutes\n", CURRENT_TIME);
    printf("--- Averages ---\n");
    printf("Avg Wait Time: %.2f minutes\n", total_wait_time / customers_served);
    printf("Avg Service Time: %.2f minutes\n", total_service_time / customers_served);
    printf("Avg Time in Bank: %.2f minutes\n", total_time_in_bank / customers_served);
    printf("\n--------------------------------------------\n");
}

void run_simulation(int num_customers, int num_tellers, float sim_time, float avg_service, int single_queue_mode) {
    CURRENT_TIME = 0.0;
    customers_served = 0;
    total_wait_time = 0.0;
    total_service_time = 0.0;
    total_time_in_bank = 0.0;
    single_queue_mode_flag = single_queue_mode;
    
    if (EventQueueHead != NULL) free_all_remaining_events();
    if (Tellers != NULL) free(Tellers);
    if (TellerLines != NULL) free_all_teller_queues();

    Tellers = (Teller*)malloc(num_tellers * sizeof(Teller));
    
    int num_queues = single_queue_mode ? 1 : num_tellers;
    TellerLines = (TellerQueue*)malloc(num_queues * sizeof(TellerQueue));
    
    for (int i = 0; i < num_tellers; i++) {
        Tellers[i].id = i + 1;
        
        if (!single_queue_mode || i == 0) {
            int q_index = single_queue_mode ? 0 : i;
            TellerLines[q_index].id = i + 1;
            TellerLines[q_index].head = NULL;
            TellerLines[q_index].tail = NULL;
            TellerLines[q_index].length = 0;
        }
    }
    
    for (int i = 0; i < num_customers; i++) {
        Customer *c = (Customer*)malloc(sizeof(Customer));
        c->id = i + 1;
        c->arrival_time = generate_arrival_time();
        c->start_service_time = 0.0;
        c->departure_time = 0.0;
        c->next = NULL;
        
        Event *e = (Event*)malloc(sizeof(Event));
        e->type = ARRIVAL;
        e->time = c->arrival_time;
        e->action = handle_customer_arrival;
        e->actor_data = c;
        insert_event_sorted(e);
    }
    
    for (int i = 0; i < num_tellers; i++) {
        Event *e = (Event*)malloc(sizeof(Event));
        e->type = TELLER_ACTION;
        // CORRECTED LINE: Uses MAX_TELLER_IDLE_MINUTES_LONG
        float initial_idle_time = (1.0 / 60.0) + (float)rand() / RAND_MAX * (MAX_TELLER_IDLE_MINUTES_LONG - (1.0 / 60.0));
        e->time = initial_idle_time;
        e->action = handle_teller_action;
        e->actor_data = &Tellers[i];
        insert_event_sorted(e);
    }
    
    printf("\nInitial events scheduled. Starting simulation...\n");
    
    while (EventQueueHead != NULL) {
        Event *current_event = remove_event_head();
        
        if (current_event->time > SIM_TIME_MINUTES) {
            free(current_event);
            break; 
        }

        CURRENT_TIME = current_event->time;
        
        current_event->action(current_event); 
        
        free(current_event);
    }
    
    free_all_remaining_events();
    free_all_teller_queues();
    
    print_statistics(single_queue_mode);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s #customers #tellers simulationTime(min) averageServiceTime(min)\n", argv[0]);
        return 1;
    }

    NUM_CUSTOMERS = atoi(argv[1]);
    NUM_TELLERS = atoi(argv[2]);
    SIM_TIME_MINUTES = atof(argv[3]);
    AVG_SERVICE_TIME_MINUTES = atof(argv[4]);
    
    if (NUM_CUSTOMERS <= 0 || NUM_TELLERS <= 0 || SIM_TIME_MINUTES <= 0.0 || AVG_SERVICE_TIME_MINUTES <= 0.0) {
        fprintf(stderr, "All parameters must be positive.\n");
        return 1;
    }

    srand(time(NULL));
    
    printf("--- Simulation Parameters ---\n");
    printf("Customers: %d, Tellers: %d, Time: %.1f min, Avg Service: %.1f min\n", 
           NUM_CUSTOMERS, NUM_TELLERS, SIM_TIME_MINUTES, AVG_SERVICE_TIME_MINUTES);

    printf("\n============================================\n");
    printf("      RUNNING SIMULATION: SEPARATE QUEUES\n");
    printf("============================================\n");
    run_simulation(NUM_CUSTOMERS, NUM_TELLERS, SIM_TIME_MINUTES, AVG_SERVICE_TIME_MINUTES, 0);
    
    printf("\n============================================\n");
    printf("        RUNNING SIMULATION: SINGLE QUEUE\n");
    printf("============================================\n");
    run_simulation(NUM_CUSTOMERS, NUM_TELLERS, SIM_TIME_MINUTES, AVG_SERVICE_TIME_MINUTES, 1);

    if (Tellers != NULL) {
        free(Tellers);
        Tellers = NULL;
    }

    return 0;
}
