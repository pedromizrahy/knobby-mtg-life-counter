// Compatibility wrapper based on Espressif esp32-camera JPEG decoder.
// SPDX-License-Identifier: Apache-2.0
#include "esp_jpg_decode_compat.h"
#include "esp_system.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/tjpgd.h"
#elif defined(CONFIG_ESP_ROM_HAS_JPEG_DECODE)
#include "rom/tjpgd.h"
#else
#include "rom/tjpgd.h"
#endif

typedef struct {
    knobby_jpg_scale_t scale;
    knobby_jpg_reader_cb reader;
    knobby_jpg_writer_cb writer;
    void *arg;
    size_t len;
    size_t index;
} knobby_jpg_decoder_t;

static unsigned int jpg_write(JDEC *decoder, void *bitmap, JRECT *rect)
{
    knobby_jpg_decoder_t *jpeg = (knobby_jpg_decoder_t *)decoder->device;
    uint16_t x = rect->left;
    uint16_t y = rect->top;
    uint16_t w = rect->right + 1U - x;
    uint16_t h = rect->bottom + 1U - y;

    if (jpeg == NULL || jpeg->writer == NULL)
        return 0;
    return jpeg->writer(jpeg->arg, x, y, w, h, (uint8_t *)bitmap) ? 1U : 0U;
}

static unsigned int jpg_read(JDEC *decoder, uint8_t *buf, unsigned int len)
{
    knobby_jpg_decoder_t *jpeg = (knobby_jpg_decoder_t *)decoder->device;

    if (jpeg == NULL || jpeg->reader == NULL)
        return 0;

    if (jpeg->len && len > (jpeg->len - jpeg->index))
        len = (unsigned int)(jpeg->len - jpeg->index);

    if (len) {
        len = (unsigned int)jpeg->reader(jpeg->arg, jpeg->index, buf, len);
        jpeg->index += len;
    }
    return len;
}

esp_err_t knobby_esp_jpg_decode(size_t len, knobby_jpg_scale_t scale,
                                knobby_jpg_reader_cb reader,
                                knobby_jpg_writer_cb writer,
                                void *arg)
{
    static uint8_t work[3100];
    JDEC decoder;
    knobby_jpg_decoder_t jpeg;
    JRESULT result;
    uint16_t output_width;
    uint16_t output_height;

    if (reader == NULL || writer == NULL)
        return ESP_ERR_INVALID_ARG;

    jpeg.len = len;
    jpeg.reader = reader;
    jpeg.writer = writer;
    jpeg.arg = arg;
    jpeg.scale = scale;
    jpeg.index = 0;

    result = jd_prepare(&decoder, jpg_read, work, sizeof(work), &jpeg);
    if (result != JDR_OK)
        return ESP_FAIL;

    output_width = (uint16_t)(decoder.width / (1U << (uint8_t)jpeg.scale));
    output_height = (uint16_t)(decoder.height / (1U << (uint8_t)jpeg.scale));

    if (!writer(arg, 0, 0, output_width, output_height, NULL))
        return ESP_FAIL;

    result = jd_decomp(&decoder, jpg_write, (uint8_t)jpeg.scale);
    if (result != JDR_OK)
        return ESP_FAIL;

    writer(arg, output_width, output_height, output_width, output_height, NULL);

    if (len && jpeg.index < len)
        jpg_read(&decoder, NULL, (unsigned int)(len - jpeg.index));

    return ESP_OK;
}
