#ifndef LIB_ACQUISITION_IMAGE_BUFFER_DEQUE_H_
#define LIB_ACQUISITION_IMAGE_BUFFER_DEQUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <lib/helpers/error_handling.h>
#include <lib/os/types.h>

typedef struct image_buffer {
    struct {
        struct image_buffer* previous;
        struct image_buffer* next;
    } deque_entry;

} image_buffer;

int image_buffer_init(image_buffer* buffer);

typedef struct image_buffer_deque {
    image_buffer head;

    bool  (*is_empty)(struct image_buffer_deque* self);
    size_t (*size)(struct image_buffer_deque* self);
    image_buffer* (*pop_front)(struct image_buffer_deque* self);
    int (*push_back)(struct image_buffer_deque* self, image_buffer* buffer);
    void (*clear)(struct image_buffer_deque* self);
    void (*pop_all)(struct image_buffer_deque* self, struct image_buffer_deque* target);
    void (*move_items_from)(struct image_buffer_deque* self, struct image_buffer_deque* source);
    int (*remove)(struct image_buffer_deque* self, image_buffer* buffer);

} image_buffer_deque;

int image_buffer_deque_init(image_buffer_deque * deque);

#ifdef __cplusplus
}
#endif

#endif