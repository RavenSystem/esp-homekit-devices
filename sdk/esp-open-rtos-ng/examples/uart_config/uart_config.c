/* A basic example that demonstrates how UART can be configured.
   Outputs some test data with 100baud, 1.5 stopbits, even parity bit to UART1
   (GPIO2; D4 for NodeMCU boards)

   This sample code is in the public domain.
 */

#include <espressif/esp_common.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <esp/uart.h>
#include <esp/uart_regs.h>

void uart_send_data(void *pvParameters){
    /* Activate UART for GPIO2 */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
    
    /* Set baud rate of UART1 to 100 (so it's easier to measure) */
    uart_set_baud(1, 100);
    
    /* Set to 1.5 stopbits */
    uart_set_stopbits(1, UART_STOPBITS_1_5);
    
    /* Enable parity bit */
    uart_set_parity_enabled(1, true);
    
    /* Set parity bit to even */
    uart_set_parity(1, UART_PARITY_EVEN);
    
    /* Repeatedly send some example packets */
    for(;;)
    {
        uart_putc(1, 0B00000000);
        uart_putc(1, 0B00000001);
        uart_putc(1, 0B10101010);
        uart_flush_txfifo(1);
    }
}

void uart_print_config(void *pvParameters){
    for(;;)
    {
        /* Get data */
        int baud = uart_get_baud(1);
        UART_StopBits stopbits = uart_get_stopbits(1);
        bool parity_enabled = uart_get_parity_enabled(1);
        UART_Parity parity = uart_get_parity(1);
        
        /* Print to UART0 */
        printf("Baud: %d ", baud);
        
        switch(stopbits){
        case UART_STOPBITS_0:
            printf("Stopbits: 0 ");
        break;
        case UART_STOPBITS_1:
            printf("Stopbits: 1 ");
        break;
        case UART_STOPBITS_1_5:
            printf("Stopbits: 1.5 ");
        break;
        case UART_STOPBITS_2:
            printf("Stopbits: 2");
        break;
        default:
            printf("Stopbits: Error");
        }
        
        printf("Parity bit enabled: %d ", parity_enabled);
        
        switch(parity){
        case UART_PARITY_EVEN:
            printf("Parity: Even");
        break;
        case UART_PARITY_ODD:
            printf("Parity: Odd");
        break;
        default:
            printf("Parity: Error");
        }
        
        printf("\n");
        
        vTaskDelay(1000.0 / portTICK_PERIOD_MS);
    }
}

void user_init(void){
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    
    xTaskCreate(uart_send_data, "tsk1", 256, NULL, 2, NULL);
    xTaskCreate(uart_print_config, "tsk2", 256, NULL, 3, NULL);
}
