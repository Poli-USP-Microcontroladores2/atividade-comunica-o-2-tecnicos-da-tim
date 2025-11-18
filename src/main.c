#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h> 
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sample, CONFIG_LOG_DEFAULT_LEVEL);


#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define MSG_SIZE 64

K_MSGQ_DEFINE(rx_msgq, MSG_SIZE, 10, 4);
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

static volatile bool rx_processar_dados = false;

void serial_cb(const struct device *dev, void *user_data)
{
    uint8_t c;
    if (!uart_irq_update(uart_dev)) { return; }
    if (!uart_irq_rx_ready(uart_dev)) { return; }

    while (uart_fifo_read(uart_dev, &c, 1) == 1) {

        if (rx_processar_dados == false) {
            continue; 
        }

        if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
            rx_buf[rx_buf_pos] = '\0';
            k_msgq_put(&rx_msgq, &rx_buf, K_NO_WAIT);
            rx_buf_pos = 0;
        } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
            rx_buf[rx_buf_pos++] = c;
        }
    }
}

void print_uart(char *buf)
{
    int msg_len = strlen(buf);
    for (int i = 0; i < msg_len; i++) {
        uart_poll_out(uart_dev, buf[i]);
    }
}

void thread_tx_burst(void)
{
    char tx_buf[MSG_SIZE];
    int loop_counter = 0;
    const int num_tx = 3; 
    
    while (1) {
        k_sleep(K_SECONDS(5));

        LOG_INF("Loop %d: Sending %d packets", loop_counter, num_tx);
        for (int i = 0; i < num_tx; i++) {
            snprintk(tx_buf, MSG_SIZE, "Loop %d: Packet: %d\r\n", loop_counter, i);
            print_uart(tx_buf);
        }

        rx_processar_dados = !rx_processar_dados; 
        
        if (rx_processar_dados) {
            LOG_INF("RX is now enabled");

            rx_buf_pos = 0; 
        } else {
            LOG_INF("RX is now disabled");
        }
        
        loop_counter++;
    }
}

int main(void)
{
    char rx_processing_buf[MSG_SIZE];

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("Dispositivo UART não encontrado!");
        return 0;
    }

    uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

    uart_irq_rx_enable(uart_dev);

    LOG_INF("RX is (logically) disabled");

    while (1) {
        if (k_msgq_get(&rx_msgq, &rx_processing_buf, K_FOREVER) == 0) {
            LOG_HEXDUMP_INF(rx_processing_buf, strlen(rx_processing_buf), "RX_RDY");
        }
    }
    return 0;
}

K_THREAD_DEFINE(tx_thread_id, 1024, thread_tx_burst, NULL, NULL, NULL,
                7, 0, 0);