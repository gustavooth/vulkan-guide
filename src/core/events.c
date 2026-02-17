#include "events.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct registered_listener {
  void* linstener;
  PFN_on_event callback;
} registered_listener;

typedef struct registered_event {
  registered_listener* listeners;
  u32 listeners_count;
} registered_event;

#define MAX_EVENT_CODE 4096
#define MAX_LISTENERS 100

typedef struct events_state {
  registered_event events[MAX_EVENT_CODE];
} events_state;

static events_state state;

b8 event_initialize() {
  printf("Events system initialized!\n");
  return true;
}

void event_shutdown() {
  for (u16 i = 0; i < MAX_EVENT_CODE; i++)
  {
    if (state.events[i].listeners != 0) {
      free(state.events[i].listeners);
    }
  }
}

void event_register(u16 code, void* listener, PFN_on_event on_event) {
  if(state.events[code].listeners == 0) {
    state.events[code].listeners = malloc(sizeof(registered_listener) * MAX_LISTENERS);
    state.events[code].listeners_count = 0;
  }

  registered_listener lis = {listener, on_event};
  state.events[code].listeners[state.events[code].listeners_count] = lis;
  state.events[code].listeners_count += 1;
}

b8 event_fire(u16 code, void* sender, EventContext data) {
  if (state.events[code].listeners ==  0) return false;
  
  for (u16 i = 0; i < state.events[code].listeners_count; i++)
  {
    if (state.events[code].listeners[i].callback(code, sender, data))
    {
      return true;
    }
  }

  return false;
}