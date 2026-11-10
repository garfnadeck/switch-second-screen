#include "hardware/es3c35p_touch.h"

#if defined(ARDUINO_ARCH_ESP32)

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hardware/es3c35p_pins.h"

static const char * TAG = "es3c35p-touch";

#define I2C_PORTA       I2C_NUM_0
#define REG_TOUCH_INFO  0x0010      // bit 0x08 = ha dados; confirmado com ACK (escreve 0x00)
#define REG_TOUCH_PT0   0x0014      // 7 bytes por ponto
#define INT_BAIXO_MAX_MS 30         // INT baixo por mais tempo que isso sem dados = chip travou

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static int64_t s_int_baixo_desde = 0;   // us, 0 = INT esta alto
static int      s_erro = -1;            // ultimo esp_err_t da leitura (-1 = ainda nao leu)
static uint8_t  s_status = 0xFF;        // ultimo byte de status (0xFF = ainda nao leu)

static esp_err_t regLer(uint16_t reg, uint8_t * buf, size_t len)
{
    uint8_t w[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    return i2c_master_transmit_receive(s_dev, w, sizeof(w), buf, len, pdMS_TO_TICKS(100));
}

static void pulsoReset()
{
    gpio_set_level((gpio_num_t)ES3C35P_TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)ES3C35P_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

namespace es3c35p_touch {

bool init()
{
    if (s_dev) return true;

    i2c_master_bus_config_t bus = {};
    bus.i2c_port = I2C_PORTA;
    bus.sda_io_num = (gpio_num_t)ES3C35P_TOUCH_SDA;
    bus.scl_io_num = (gpio_num_t)ES3C35P_TOUCH_SCL;
    bus.clk_source = I2C_CLK_SRC_DEFAULT;
    bus.glitch_ignore_cnt = 7;
    bus.flags.enable_internal_pullup = 1;
    if (i2c_new_master_bus(&bus, &s_bus) != ESP_OK) {
        ESP_LOGE(TAG, "falha ao criar o barramento I2C");
        return false;
    }

    gpio_config_t rst = {};
    rst.pin_bit_mask = 1ULL << ES3C35P_TOUCH_RST;
    rst.mode = GPIO_MODE_OUTPUT;
    gpio_config(&rst);
    pulsoReset();

    gpio_config_t in = {};
    in.pin_bit_mask = 1ULL << ES3C35P_TOUCH_INT;
    in.mode = GPIO_MODE_INPUT;
    in.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&in);

    i2c_device_config_t dev = {};
    dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev.device_address = ES3C35P_TOUCH_ADDR;
    dev.scl_speed_hz = 400000;
    if (i2c_master_bus_add_device(s_bus, &dev, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "chip nao respondeu em 0x%02X", ES3C35P_TOUCH_ADDR);
        return false;
    }

    // add_device nao checa o chip: o probe confirma que ele responde (ACK) no endereco
    if (i2c_master_probe(s_bus, ES3C35P_TOUCH_ADDR, 100) != ESP_OK) {
        ESP_LOGE(TAG, "chip nao respondeu (ACK) em 0x%02X", ES3C35P_TOUCH_ADDR);
        return false;
    }

    ESP_LOGI(TAG, "touch iniciado (0x%02X, 1 ponto)", ES3C35P_TOUCH_ADDR);
    return true;
}

bool read(uint16_t * x, uint16_t * y)
{
    if (!s_dev) return false;

    uint8_t info = 0;
    s_erro = regLer(REG_TOUCH_INFO, &info, 1);
    s_status = info;

    // Os pontos sao lidos em todo ciclo, sem depender do bit de status (como na bancada)
    uint8_t ponto[7] = { 0 };
    esp_err_t err = regLer(REG_TOUCH_PT0, ponto, sizeof(ponto));

    if (info & 0x08) {
        uint8_t ack[3] = { (uint8_t)(REG_TOUCH_INFO >> 8), (uint8_t)(REG_TOUCH_INFO & 0xFF), 0x00 };
        i2c_master_transmit(s_dev, ack, sizeof(ack), pdMS_TO_TICKS(100));
    }

    // Workaround: INT baixo por mais de 30 ms sem dados novos -> reset do chip
    if (gpio_get_level((gpio_num_t)ES3C35P_TOUCH_INT) != 0) {
        s_int_baixo_desde = 0;
    } else {
        int64_t agora = esp_timer_get_time();
        if (s_int_baixo_desde == 0) s_int_baixo_desde = agora;
        if ((agora - s_int_baixo_desde) / 1000 > INT_BAIXO_MAX_MS) {
            ESP_LOGW(TAG, "INT baixo por mais de %d ms: reset do touch", INT_BAIXO_MAX_MS);
            pulsoReset();
            s_int_baixo_desde = 0;
        }
    }

    if (err != ESP_OK || !(ponto[0] & 0x80)) return false;     // bit 7 = ponto valido
    *x = ((ponto[0] & 0x3F) << 8) | ponto[1];
    *y = ((ponto[2] & 0x3F) << 8) | ponto[3];
    return true;
}

int     ultimoErro()   { return s_erro; }
uint8_t ultimoStatus() { return s_status; }

}  // namespace es3c35p_touch

#else  // fora do ESP32: sem toque

namespace es3c35p_touch {
bool init() { return false; }
bool read(uint16_t *, uint16_t *) { return false; }
int     ultimoErro()   { return -1; }
uint8_t ultimoStatus() { return 0; }
}  // namespace es3c35p_touch

#endif
