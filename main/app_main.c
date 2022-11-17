// TODO: have a heartbeat task -> wifi stats
// TODO: send wifi reconnect on reconnect
// TODO: publish logs to console/syslog
// TODO: remove hardcoded variables
// TODO: cast variables to their types

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "protocol_examples_common.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <driver/spi_master.h>

#define TOPIC_MAX_LEN 250
#define DATA_MAX_LEN 250

static const char *TAG = "VIRTUAL_ESP";
char uri[] = "mqtt://mqtt.bucknell.edu/";
char str_mac_addr[18];

esp_mqtt_client_handle_t client;
spi_device_handle_t device_handle_1;
spi_device_handle_t device_handle_2;

int spi_busy = 0;

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_eclipseprojects_io_pem_start[] = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_eclipseprojects_io_pem_start[] asm("_binary_mqtt_eclipseprojects_io_pem_start");
#endif
extern const uint8_t mqtt_eclipseprojects_io_pem_end[] asm("_binary_mqtt_eclipseprojects_io_pem_end");

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    int gpio_num = (int)arg;
    char gpio_str[3];
    itoa(gpio_num, gpio_str, 10);
    esp_mqtt_client_enqueue(client, "/console/gpio/isr_handler", gpio_str, 0, 0, 0, true);
}

static void spi_command_handler(char *t_parser, char data_str[])
{
    if (strcmp(t_parser, "bus_initialize") == 0)
    {
        int host_id;
        int mosi_io_num;
        int miso_io_num;
        int sclk_io_num;
        int quadwp_io_num;
        int quadhd_io_num;
        int max_transfer_sz;
        int flags;
        int intr_flags;
        int dma_chan;
        ESP_LOGI(TAG, "BUS_INIT");
        if (sscanf(data_str, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i", &host_id, &mosi_io_num, &miso_io_num, &sclk_io_num, &quadwp_io_num, &quadhd_io_num, &max_transfer_sz, &flags, &intr_flags, &dma_chan))
        {
            spi_bus_config_t bus_config = {
                .mosi_io_num = mosi_io_num,
                .miso_io_num = miso_io_num,
                .sclk_io_num = sclk_io_num,
                .quadwp_io_num = quadwp_io_num,
                .max_transfer_sz = max_transfer_sz
            };
            spi_bus_initialize((spi_host_device_t)host_id, &bus_config, (spi_dma_chan_t)dma_chan);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "bus_add_device") == 0)
    {
        int host_id;
        int command_bits;
        int address_bits;
        int dummy_bits;
        int mode;
        int spics_io_num;
        int clock_speed_hz;
        int queue_size;
        int handle_id;
        int flags;
        ESP_LOGI(TAG, "BUS_ADD_DEVICE");
        if (sscanf(data_str, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i", &host_id, &command_bits, &address_bits, &dummy_bits, &mode, &spics_io_num, &clock_speed_hz, &queue_size, &handle_id, &flags))
        {
            spi_device_interface_config_t dev_config = {
                .command_bits = command_bits,
                .address_bits = address_bits,
                .dummy_bits = dummy_bits,
                .mode = mode,
                .spics_io_num = spics_io_num,
                .clock_speed_hz = clock_speed_hz,
                .queue_size = queue_size,
                .flags = flags
            };
            if (handle_id == 1) {
                spi_bus_add_device((spi_host_device_t)host_id, &dev_config, &device_handle_1);
                ESP_LOGI(TAG, "HANDLE_1_CONFIG");
            }  
            else if (handle_id == 2) {
                spi_bus_add_device((spi_host_device_t)host_id, &dev_config, &device_handle_2);
                ESP_LOGI(TAG, "HANDLE_2_CONFIG");
            }
            else {
                ESP_LOGI(TAG, "HANDLE_INVALID_CONFIG");
            }
            
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "device_transmit") == 0)
    {
        ESP_LOGI(TAG, "DEVICE_TRANSMIT");
        int length;
        int rxlength;
        int handle_id;
        esp_err_t err = ESP_OK;

        const char d_delim[2] = ",";
        char *d_parser;

        d_parser = strtok(data_str, d_delim);
        length = atoi(d_parser);
        d_parser = strtok(NULL, d_delim);
        rxlength = atoi(d_parser);
        d_parser = strtok(NULL, d_delim);
        handle_id = atoi(d_parser);
        d_parser = strtok(NULL, d_delim);

        void *tx_buffer = heap_caps_malloc(length, MALLOC_CAP_DMA | MALLOC_CAP_32BIT);
        void *rx_buffer = heap_caps_malloc(rxlength, MALLOC_CAP_DMA | MALLOC_CAP_32BIT);
        if (tx_buffer == NULL)
        {
            ESP_LOGE(TAG, "HEAP_CAPS_MALLOC");
            err = ESP_ERR_NO_MEM;
            return err;
        }
        memset(tx_buffer, 0, length);

        int i = 0;
        while ((i < length) && (d_parser != NULL))
        {
            ((uint8_t *)tx_buffer)[i] = (uint8_t)atoi(d_parser);
            d_parser = strtok(NULL, d_delim);
            i++;
        }
        spi_transaction_t trans_desc = {
            .length = length * 8,
            .tx_buffer = tx_buffer,
            .rxlength = rxlength * 8,
            .rx_buffer = rx_buffer,
        };

        spi_busy = 1;
        if (handle_id == 1) {
            err = spi_device_transmit(device_handle_1, &trans_desc);;
            ESP_LOGI(TAG, "HANDLE_ID_1 transmit");
        }  
        else if (handle_id == 2) {
            err = spi_device_transmit(device_handle_2, &trans_desc);
            ESP_LOGI(TAG, "HANDLE_ID_2 transmit");
        }
        else {
            ESP_LOGI(TAG, "HANDLE_ID_INVALID transmit");
        }
        if (err != ESP_OK)
            ESP_LOGE(TAG, "DEVICE_TRANSMIT: %s", esp_err_to_name(err));

        if (rxlength > 0)
        {
            char msg_str[3 * rxlength + 1];
            int msg_i = 0;
            for (int i = 0; i < rxlength; i++)
            {
                msg_i += sprintf(&msg_str[msg_i], "%i,", ((uint8_t *)rx_buffer)[i]);
                ESP_LOGI(TAG, "DEVICE_TRANSMIT message_read=%i", ((uint8_t *)rx_buffer)[i]);
            }
            int msg_id = esp_mqtt_client_publish(client, "/console/spi/device_transmit", msg_str, 0, 2, 0);
            ESP_LOGI(TAG, "DEVICE_TRANSMIT published, MSG_ID=%d", msg_id);
        }

        heap_caps_free(tx_buffer);
        heap_caps_free(rx_buffer);
        spi_busy = 0;
    }
    else
    {
        ESP_LOGE(TAG, "%s", t_parser);
        ESP_LOGE(TAG, "BAD_SPI_REQUEST");
        while (1);
    }
}

static void gpio_command_handler(char *t_parser, char data_str[])
{
    int gpio_num;
    int level;
    int mode;
    int msg;
    int msg_id;
    char msg_str[2];
    if (strcmp(t_parser, "reset_pin") == 0)
    {
        ESP_LOGI(TAG, "RESET_PIN_FUNC");
        if (sscanf(data_str, "%i", &gpio_num))
        {
            gpio_reset_pin(gpio_num);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "set_direction") == 0)
    {
        ESP_LOGI(TAG, "SET_DIRECTION_FUNC");
        if (sscanf(data_str, "%i,%i", &gpio_num, &mode))
        {
            gpio_set_direction(gpio_num, (gpio_mode_t)mode);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "set_level") == 0)
    {
        ESP_LOGI(TAG, "SET_LEVEL_FUNC");
        if (sscanf(data_str, "%i,%i", &gpio_num, &level))
        {
            gpio_set_level(gpio_num, level);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "get_level") == 0)
    {
        ESP_LOGI(TAG, "GET_LEVEL_FUNC");
        if (sscanf(data_str, "%i", &gpio_num))
        {
            msg = gpio_get_level(gpio_num);
            itoa(msg, msg_str, 10);
            msg_id = esp_mqtt_client_publish(client, "/console/gpio/get_level", msg_str, 0, 2, 0);
            ESP_LOGI(TAG, "GET_LEVEL_FUNC published, MSG_ID=%d", msg_id);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "set_intr_type") == 0)
    {
        ESP_LOGI(TAG, "SET_INTR_TYPE");
        int intr_type;
        if (sscanf(data_str, "%i,%i", &gpio_num, &intr_type))
        {
            gpio_set_intr_type(gpio_num, (gpio_int_type_t)intr_type);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "install_isr_service") == 0)
    {
        ESP_LOGI(TAG, "INSTALL_ISR_SERVICE");
        gpio_install_isr_service(0);
    }
    else if (strcmp(t_parser, "uninstall_isr_service") == 0)
    {
        ESP_LOGI(TAG, "UNINSTALL_ISR_SERVICE");
        gpio_uninstall_isr_service();
    }
    else if (strcmp(t_parser, "isr_handler_add") == 0)
    {
        ESP_LOGI(TAG, "ISR_ADD_FUNC");
        if (sscanf(data_str, "%i", &gpio_num))
        {
            gpio_isr_handler_add(gpio_num, gpio_isr_handler, (void *)gpio_num);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "isr_handler_remove") == 0)
    {
        ESP_LOGI(TAG, "ISR_REMOVE_FUNC");
        if (sscanf(data_str, "%i", &gpio_num))
        {
            gpio_isr_handler_remove(gpio_num);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else if (strcmp(t_parser, "pulldown_en") == 0)
    {
        ESP_LOGI(TAG, "PULLDOWN_EN");
        if (sscanf(data_str, "%i", &gpio_num))
        {
            gpio_pulldown_en(gpio_num);
        }
        else
        {
            ESP_LOGE(TAG, "BAD_DATA");
        }
    }
    else
    {
        ESP_LOGE(TAG, "%s", t_parser);
        ESP_LOGE(TAG, "BAD_GPIO_REQUEST");
        while (1);
    }
}

static void esp_command_handler(esp_mqtt_event_handle_t event)
{
    char* topic_str = malloc(TOPIC_MAX_LEN);
    char* data_str = malloc(DATA_MAX_LEN);

    const char t_delim[2] = "/";

    char *t_parser;

    strncpy(topic_str, event->topic, event->topic_len);
    strncpy(data_str, event->data, event->data_len);
    topic_str[event->topic_len] = '\0';
    data_str[event->data_len] = '\0';

    t_parser = strtok(topic_str, t_delim);

    if (strcmp(t_parser, "gpio") == 0)
    {
        ESP_LOGI(TAG, "GPIO_COMMAND_READ");
        t_parser = strtok(NULL, t_delim);
        gpio_command_handler(t_parser, data_str);
    }
    else if (strcmp(t_parser, "spi") == 0)
    {
        ESP_LOGI(TAG, "SPI_COMMAND_READ");
        t_parser = strtok(NULL, t_delim);
        while (spi_busy == 1)
        {
            ESP_LOGI(TAG, "SPI_BUSY");
        }
        spi_command_handler(t_parser, data_str);
    }
    else
    {
        ESP_LOGE(TAG, "%s", t_parser);
        ESP_LOGE(TAG, "BAD_GENERAL_REQUEST");
        while (1);
    }
    
    free(topic_str);
    free(data_str);
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    int msg_id;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/gpio/#", 0);
        msg_id = esp_mqtt_client_subscribe(client, "/ledc/#", 2);
        msg_id = esp_mqtt_client_subscribe(client, "/spi/#", 2);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/console/boot", str_mac_addr, 0, 2, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("TOPIC_LEN=%d\r\n", event->topic_len);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        printf("DATA_LEN=%d\r\n", event->data_len);
        esp_command_handler(event);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void get_mac_address(char *str_mac_addr)
{
    uint8_t base_mac_addr[6] = {0};
    esp_err_t ret = ESP_OK;
    ret = esp_efuse_mac_get_default(base_mac_addr);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get base MAC address from EFUSE BLK0. (%s)", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Aborting");
        abort();
    }
    else
    {
        ESP_LOGI(TAG, "Base MAC Address read from EFUSE BLK0");
        int i = 0;
        int si = 0;
        for (i = 0; i < 5; i++)
            si += sprintf(&str_mac_addr[si], "%x", base_mac_addr[i]);
    }
}

static void mqtt_app_start(void)
{
    get_mac_address(str_mac_addr);
    ESP_LOGI(TAG, "[APP] System MAC Address: %s", str_mac_addr);

    strcat(uri, str_mac_addr);
    ESP_LOGI(TAG, "[APP] MQTT URI = %s", uri);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = uri,
        .cert_pem = (const char *)mqtt_eclipseprojects_io_pem_start,
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
