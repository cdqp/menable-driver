#include <lib/uiq/uiq_helper.h>
#include <lib/helpers/bits.h>
#include <lib/helpers/helper.h>

#include <wdm.h>

void men_get_uiq_timestamp(uiq_timestamp* out_timestamp) {

    LARGE_INTEGER counter = KeQueryPerformanceCounter(NULL);

    const size_t num_parts = ARRAY_SIZE(out_timestamp->data);
    const size_t num_bits_in_counter = sizeof(counter.QuadPart) * 8;

    for (int i = 0; i < num_parts; ++i) {
        const uint8_t bits_to = num_bits_in_counter - (i * UIQ_NUM_PAYLOAD_BITS);
        const uint8_t bits_from = (bits_to - UIQ_NUM_PAYLOAD_BITS) + 1;
        out_timestamp->data[i] = GET_BITS_64(counter.QuadPart, bits_from, bits_to);
    }
}