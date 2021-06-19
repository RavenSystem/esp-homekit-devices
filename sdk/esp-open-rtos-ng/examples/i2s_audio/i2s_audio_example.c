/* i2s_audio_example.c - Plays wav file from spiffs.
 *
 * This example demonstrates how to use I2S with DMA to output audio.
 * The example is tested with TDA5143 16 bit DAC. But should work with
 * any I2S DAC.
 *
 * The example reads a file with name "sample.wav" from the file system and
 * feeds audio samples into DMA subsystem which outputs it into I2S bus.
 * Currently only 44100 Hz 16 bit 2 channel audio is supported.
 *
 * In order to test this example you need to place a file with name "sample.wav"
 * into directory "files". This file will be uploaded into spiffs on the device.
 * The size of the sample file must be less than 1MB to fit into spiffs image.
 * The format of the sample file must be 44100Hz, 16bit, 2 channels.
 * Also you need a DAC connected to ESP8266 to convert I2S stream to analog
 * output. Three wire must be connected: DATA, WS, CLOCK.
 *
 * This sample code is in the public domain.,
 */
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "esp8266.h"

#include "fcntl.h"
#include "unistd.h"

#include <stdio.h>
#include <stdint.h>

#include "esp_spiffs.h"
#include "i2s_dma/i2s_dma.h"

// Very simple WAV header, ignores most fields
typedef struct __attribute__((packed)) {
    uint8_t ignore_0[22];
    uint16_t num_channels;
    uint32_t sample_rate;
    uint8_t ignore_1[6];
    uint16_t bits_per_sample;
    uint8_t ignore_2[4];
    uint32_t data_size;
    uint8_t data[];
} dumb_wav_header_t;

// When samples are not sent fast enough underrun condition occurs
volatile uint32_t underrun_counter = 0;

#define DMA_BUFFER_SIZE         2048
#define DMA_QUEUE_SIZE          8

// Circular list of descriptors
static dma_descriptor_t dma_block_list[DMA_QUEUE_SIZE];

// Array of buffers for circular list of descriptors
static uint8_t dma_buffer[DMA_QUEUE_SIZE][DMA_BUFFER_SIZE];

// Queue of empty DMA blocks
static QueueHandle_t dma_queue;

/**
 * Create a circular list of DMA descriptors
 */
static inline void init_descriptors_list()
{
    memset(dma_buffer, 0, DMA_QUEUE_SIZE * DMA_BUFFER_SIZE);

    for (int i = 0; i < DMA_QUEUE_SIZE; i++) {
        dma_block_list[i].owner = 1;
        dma_block_list[i].eof = 1;
        dma_block_list[i].sub_sof = 0;
        dma_block_list[i].unused = 0;
        dma_block_list[i].buf_ptr = dma_buffer[i];
        dma_block_list[i].datalen = DMA_BUFFER_SIZE;
        dma_block_list[i].blocksize = DMA_BUFFER_SIZE;
        if (i == (DMA_QUEUE_SIZE - 1)) {
            dma_block_list[i].next_link_ptr = &dma_block_list[0];
        } else {
            dma_block_list[i].next_link_ptr = &dma_block_list[i + 1];
        }
    }

    // The queue depth is one smaller than the amount of buffers we have,
    // because there's always a buffer that is being used by the DMA subsystem
    // *right now* and we don't want to be able to write to that simultaneously
    dma_queue = xQueueCreate(DMA_QUEUE_SIZE - 1, sizeof(uint8_t*));
}

// DMA interrupt handler. It is called each time a DMA block is finished processing.
static void dma_isr_handler(void *args)
{
    portBASE_TYPE task_awoken = pdFALSE;

    if (i2s_dma_is_eof_interrupt()) {
        dma_descriptor_t *descr = i2s_dma_get_eof_descriptor();

        if (xQueueIsQueueFullFromISR(dma_queue)) {
            // List of empty blocks is full. Sender don't send data fast enough.
            int dummy;
            underrun_counter++;
            // Discard top of the queue
            xQueueReceiveFromISR(dma_queue, &dummy, &task_awoken);
        }
        // Push the processed buffer to the queue so sender can refill it.
        xQueueSendFromISR(dma_queue, (void*)(&descr->buf_ptr), &task_awoken);
    }
    i2s_dma_clear_interrupt();

    portEND_SWITCHING_ISR(task_awoken);
}

static bool play_data(int fd)
{
    uint8_t *curr_dma_buf;

    // Get a free block from the DMA queue. This call will suspend the task
    // until a free block is available in the queue.
    if (xQueueReceive(dma_queue, &curr_dma_buf, portMAX_DELAY) == pdFALSE) {
        // Or timeout occurs
        printf("Cound't get free blocks to push data\n");
    }

    int read_bytes = read(fd, curr_dma_buf, DMA_BUFFER_SIZE);
    if (read_bytes <= 0) {
        return false;
    }

    return true;
}

void play_task(void *pvParameters)
{
    esp_spiffs_init();
    if (esp_spiffs_mount() != SPIFFS_OK) {
        printf("Error mount SPIFFS\n");
    }

    int fd = open("sample.wav", O_RDONLY);

    if (fd < 0) {
        printf("Error opening file\n");
        return;
    }

    dumb_wav_header_t wav_header;
    read(fd, (void*)&wav_header, sizeof(wav_header));
    printf("Number of channels: %d\n", wav_header.num_channels);
    printf("Bits per sample: %d\n", wav_header.bits_per_sample);
    printf("Sample rate: %d\n", wav_header.sample_rate);
    printf("Data size: %d\n", wav_header.data_size);

    if (wav_header.bits_per_sample != 16) {
        printf("Only 16 bit per sample is supported\n");
        return;
    }

    if (wav_header.num_channels != 2) {
        printf("Only 2 channels is supported\n");
        return;
    }

    i2s_clock_div_t clock_div = i2s_get_clock_div(
            wav_header.sample_rate * 2 * 16);

    printf("i2s clock dividers, bclk=%d, clkm=%d\n",
            clock_div.bclk_div, clock_div.clkm_div);

    i2s_pins_t i2s_pins = {.data = true, .clock = true, .ws = true};

    i2s_dma_init(dma_isr_handler, NULL, clock_div, i2s_pins);

    while (1) {
        init_descriptors_list();

        i2s_dma_start(dma_block_list);
        lseek(fd, sizeof(dumb_wav_header_t), SEEK_SET);

        while (play_data(fd)) {};
        i2s_dma_stop();

        vQueueDelete(dma_queue);

        printf("underrun counter: %d\n", underrun_counter);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    close(fd);
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    xTaskCreate(play_task, "test_task", 1024, NULL, 2, NULL);
}
