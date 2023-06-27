#ifndef LIB_ACQUISITION_IMAGE_BUFFER_MANAGER_H_
#define LIB_ACQUISITION_IMAGE_BUFFER_MANAGER_H_

#include "image_buffer_deque.h"

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct image_buffer_manager {
    image_buffer_deque free;
    image_buffer_deque ready;
    image_buffer_deque hot;
    image_buffer_deque grabbed;
    image_buffer_deque done;

    size_t (*size_of)(image_buffer_deque* deque);
    void(*clear_all)(struct image_buffer_manager* self);
} image_buffer_manager;

int image_buffer_manager_init(image_buffer_manager* manager);

#ifdef __cplusplus
    }
#endif

#endif // LIB_ACQUISITION_IMAGE_BUFFER_MANAGER_H_