#include <stdio.h>
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

QueueHandle_t twai_queue; // Tworzymy rurę do transportu odebranych ramek CAN
twai_frame_t odebrana_ramka; // Tworzymy strukturę do przechowywania odebranej ramki CAN

twai_node_handle_t node_hdl = NULL; // Tworzymy uchwyt do kontrolera TWAI dla konkretnego węzła

// Konfiguracja węzła TWAI dla wbudowanego kontrolera
twai_onchip_node_config_t node_config = {
    .io_cfg.tx = 4,             // TWAI TX GPIO pin
    .io_cfg.rx = 5,             // TWAI RX GPIO pin
    .bit_timing.bitrate = 500000,  // 500 kbps bitrate
    .tx_queue_depth = 5,        // Transmit queue depth set to 5
};

// Funkcja callback dla zdarzenia "RX done" (odebrano ramkę)
static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    static uint8_t recv_buff[8]; // Tworzymy bufor do przechowywania odebranej ramki CAN
    twai_frame_t rx_frame = {
        .buffer = recv_buff, // Wskazujemy bufor do przechowywania odebranej ramki
        .buffer_len = sizeof(recv_buff), // Ustawiamy długość bufora, w tym przypadku 8 bajtów dla standardowej ramki CAN
    };
    // Jeżeli odebrano ramkę, umieszczamy ją w kolejce do dalszego przetwarzania w głównej pętli programu
    if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {
        xQueueSendFromISR(twai_queue, &rx_frame, NULL); // Umieszczamy odebraną ramkę w kolejce do dalszego przetwarzania
    }
    return false; // Zwracamy false, aby nie odblokowywać wyższych priorytetów zadań
}

void app_main() {

    // Tworzymy strukturę z funkcjami callback dla zdarzeń TWAI
    twai_event_callbacks_t user_cbs = {
        .on_rx_done = twai_rx_cb, // Rejestrujemy funkcję callback dla zdarzenia "RX done"
    };

    // Tworzymy kolejkę do przechowywania odebranych ramek CAN
    twai_queue = xQueueCreate(10, sizeof(twai_frame_t) );

    // Tworzymy nowy węzeł TWAI z konfiguracją dla wbudowanego kontrolera
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));
    // Rejestrujemy funkcje callback dla zdarzeń TWAI, w tym przypadku tylko dla zdarzenia "RX done"
    twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL);

    // Włączamy węzeł TWAI, aby rozpocząć komunikację
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    // Główna pętla programu, w której odbieramy i przetwarzamy odebrane ramki CAN
    while(1) {
        if(xQueueReceive(twai_queue, &odebrana_ramka, portMAX_DELAY ) == pdTRUE) {
        // Handle received data
        printf("Ramka CAN odebrana: %d\n", (int)odebrana_ramka.header.id); // Wyświetlamy ID odebranej ramki CAN
        }
    }
}
