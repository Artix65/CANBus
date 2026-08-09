#include <stdio.h>
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Define a structure to store received CAN frames (because speed is too high)
typedef struct {
    twai_frame_t frame; // Parameters (ID, length), interial TWAI frame structure
    uint8_t dane[8];    // Reserve buffer for received CAN frame data
} moja_ramka_can_t;

twai_node_handle_t node_hdl = NULL;
twai_onchip_node_config_t node_config = {
    .io_cfg.tx = 4,             // TWAI TX GPIO pin
    .io_cfg.rx = 5,             // TWAI RX GPIO pin
    .bit_timing.bitrate = 500000,  // 500 kbps bitrate
    .tx_queue_depth = 5,        // Transmit queue depth set to 5
};

// Create a queue to store received CAN frames
QueueHandle_t twai_queue;

// Define function for reciving CAN frames from the TWAI controller
static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    moja_ramka_can_t paczka;
    paczka.frame.buffer = paczka.dane; // Set the buffer to store received CAN frame data
    paczka.frame.buffer_len = sizeof(paczka.dane); // Set the buffer length to 8 bytes for standard CAN frame

    if (ESP_OK == twai_node_receive_from_isr(handle, &paczka.frame)) {
        // receive ok, do something here

        // Here we will use RTOS queue to send the received frame to the main loop for further processing
        xQueueSendFromISR(twai_queue, &paczka, NULL);
    }
    return false;
}

// Define a callback function for the "RX done" event
twai_event_callbacks_t user_cbs = {
    .on_rx_done = twai_rx_cb,
};

void app_main() {

    twai_queue = xQueueCreate(10, sizeof(moja_ramka_can_t)); // Create a queue to store received CAN frames

    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    // Register the callback function for the "RX done" event
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL));

    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    // After enabling the TWAI controller

    while(1) {
        moja_ramka_can_t odebrana_paczka;

        if(xQueueReceive(twai_queue, &odebrana_paczka, portMAX_DELAY ) == pdTRUE) {
        // Handle received data
        printf("Ramka CAN odebrana: %d\n", (int)odebrana_paczka.frame.header.id); // Wyświetlamy ID odebranej ramki CAN
        for(int i = 0; i < odebrana_paczka.frame.buffer_len; i++) {
            printf("Dane[%d]: %02X ", i, odebrana_paczka.dane[i]); // Wyświetlamy dane odebranej ramki CAN w formacie szesnastkowym
        }
        printf("\n");
        }
    }
}
