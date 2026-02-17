#pragma once
#include "defines.h"

typedef struct EventContext {
  // 128 bytes
  union {
    i64 i64[2];
    u64 u64[2];
    f64 f64[2];

    i32 i32[4];
    u32 u32[4];
    f32 f32[4];

    i16 i16[8];
    u16 u16[8];

    i8 i8[16];
    u8 u8[16];

    char c[16];
  } data;
} EventContext;

typedef b8 (*PFN_on_event)(u16 code, void* sender, EventContext data);

b8 event_initialize();
void event_shutdown();

/**
 * Register to listen for when events are sent with the provided code.
 * @param code The event code to listen for.
 * @param listener A pointer to a listener instance. Can be 0/NULL.
 * @param on_event The callback function pointer to be invoked when the event code is fired.
 */
void event_register(u16 code, void* listener, PFN_on_event on_event);

/**
 * Unregister from listening for when events are sent with the provided code. If no matching
 * registration is found, this function returns FALSE.
 * @param code The event code to stop listening for.
 * @param on_event The callback function pointer to be unregistered.
 * @returns FALSE if the event is not registered.
 */
b8 event_unregister(u16 code, PFN_on_event on_event);

/**
 * Fires an event to listeners of the given code. If an event handler returns 
 * TRUE, the event is considered handled and is not passed on to any more listeners.
 * @param code The event code to fire.
 * @param sender A pointer to the sender. Can be 0/NULL.
 * @param data The event data.
 * @returns TRUE if a listener returns true.
 */
b8 event_fire(u16 code, void* sender, EventContext data);

typedef enum SystemEventCode {
    EVENT_CODE_APPLICATION_QUIT = 0x01,
    EVENT_CODE_RESIZED = 0x02,

    MAX_EVENT_CODE = 0xFF
} SystemEventCode;