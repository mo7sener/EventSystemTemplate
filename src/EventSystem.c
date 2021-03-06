#include "EventSystem.h"
#include "assert.h"
#define HAVE_STRUCT_TIMESPEC
#include "pthread.h"
#include "stdio.h"

static void event_system_init();
static void event_system_deinit();
static void event_system_add_listener(struct EventListener *listener, enum EventType type);
static void event_system_remove_listener(struct EventListener *listener);
static inline void event_system_remove_event_sub(struct EventListener *listener, enum EventType type);
static void event_system_dispatch_event(void *event);
static void event_system_update();

struct event_system_struct EventSystem = {
	.init = event_system_init,
	.deinit = event_system_deinit,
	.addListener = event_system_add_listener,
	.removeListener = event_system_remove_listener,
	.unsub = event_system_remove_event_sub,
	.update = event_system_update,
	.dispatch = event_system_dispatch_event,
};

static void event_system_init()
{
	event_buffer_pair_init(&EventSystem.bufferPair);
	vec_init(&EventSystem.listeners);
	loop_event_types {
		vec_init(&EventSystem.eventListeners[type]);
	}
}

static void event_system_deinit()
{
	event_buffer_pair_deinit(&EventSystem.bufferPair);
	vec_deinit(&EventSystem.listeners);
	loop_event_types {
		vec_deinit(&EventSystem.eventListeners[type]);
	}
}

static void event_system_add_listener(struct EventListener *listener, enum EventType type)
{
	unsigned int idx;
	vec_find(&EventSystem.listeners, listener, idx);
	if(idx == -1)
		assert(!vec_push(&EventSystem.listeners, listener));
	vec_find(&EventSystem.eventListeners[type], listener, idx);
	if(idx == -1)
		assert(!vec_push(&EventSystem.eventListeners[type], listener));
}

static inline void event_system_remove_event_sub(struct EventListener *listener, enum EventType type)
{
	unsigned int idx;
	vec_find(&EventSystem.eventListeners[type], listener, idx);
	if(idx != -1)
		vec_remove(&EventSystem.eventListeners[type], listener);
}

static void event_system_remove_listener(struct EventListener *listener)
{
	loop_event_types {
		event_system_remove_event_sub(listener, type);
	}
	unsigned int idx;
	vec_find(&EventSystem.listeners, listener, idx);
	if(idx != -1)
		vec_remove(&EventSystem.listeners, listener);
}

static inline void handle_event(enum EventType type, void *event)
{
	struct EventListener *listener; unsigned int __idx;
	vec_foreach(&EventSystem.eventListeners[type], listener, __idx) {
		listener->handlers[type](event);
	}
}

void* handle_type(void* eventType)
{
    int type = (int)eventType;

    for(unsigned int event = 0; event < EventSystem.bufferPair.read->sizes[type]; ++event)
    {
        unsigned int __idx;
        struct EventListener *listener;

        vec_foreach(&EventSystem.eventListeners[type], listener, __idx) {
                listener->handlers[type](
                        (char*)(EventSystem.bufferPair.read->buffers[type]) + (event * EVENT_SIZE)
                );
            }
    }
    return 0;
}

static void event_system_update()
{
	event_buffer_pair_swap(&EventSystem.bufferPair);

    pthread_t threads[EVENT_TYPE_COUNT];

	loop_event_types {
	    pthread_create(threads + type, NULL, handle_type, (void*)type);
    }

    loop_event_types {
	    pthread_join(threads[type], NULL);
	}

	event_buffer_pair_clear_read(&EventSystem.bufferPair);
}

static void event_system_dispatch_event(void *event)
{
	event_buffer_pair_add_event(&EventSystem.bufferPair, event);
}
