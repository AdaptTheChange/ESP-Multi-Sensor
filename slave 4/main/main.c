/*
 * ============================================================
 *  ESP32 "SLAVE 4" - DUAL PT100 RTD + HALL SENSOR (+ LIDAR LATER)
 * ============================================================
 *
 *  Board : ESP32-WROOM-32 DevKit
 *  SDK   : ESP-IDF v5.2+ / v6.x
 *
 *  SENSORS
 *    1. PT100 RTD #1 via MAX31865 board  - SPI2 (own bus)
 *    2. PT100 RTD #2 via MAX31865 board  - SPI3 (own bus)
 *    3. Analog Hall-effect sensor        - ADC1_CH6 (GPIO34)
 *    4. VL53L4CD LiDAR                   - NOT IMPLEMENTED YET (placeholder)
 *
 * ------------------------------------------------------------
 *  FIRST-TIME PROJECT SETUP
 * ------------------------------------------------------------
 *  This code needs the "max31865" library from esp-idf-lib.
 *
 *  1. Create  main/idf_component.yml  containing:
 *
 *         dependencies:
 *           esp-idf-lib/max31865: "*"
 *
 *  2. Create  main/CMakeLists.txt  containing:
 *
 *         idf_component_register(SRCS "main.c"
 *                                INCLUDE_DIRS "."
 *                                PRIV_REQUIRES esp_driver_spi esp_driver_gpio esp_adc)
 *
 *  3. Build (internet needed the first time to download the library):
 *
 *         idf.py set-target esp32
 *         idf.py build
 *         idf.py -p <PORT> flash monitor
 *
 * ------------------------------------------------------------
 *  WIRING
 * ------------------------------------------------------------
 *  Each MAX31865 board has its own SPI bus:
 *
 *    MAX31865 pin   RTD #1 (SPI2)    RTD #2 (SPI3)
 *    ------------   --------------   --------------
 *    VIN            3V3              3V3
 *    GND            GND              GND
 *    CLK  (SCK)     GPIO 18          GPIO 14
 *    SDI  (MOSI)    GPIO 23          GPIO 13
 *    SDO  (MISO)    GPIO 19          GPIO 27
 *    CS             GPIO 25          GPIO 26
 *    RDY            not connected    not connected
 *
 *  SDI is the board's input, so it connects to the ESP32 MOSI pin.
 *  SDO is the board's output, so it connects to the ESP32 MISO pin.
 *
 *  PT100 probe (4-wire): connect the two wires of one lead to F+ and
 *  RTD+, and the two wires of the other lead to RTD- and F-.
 *  The board's solder jumpers must match the wiring mode.
 *  If you use 2-wire or 3-wire probes, change RTD_WIRING below.
 *
 *  Hall sensor:
 *    VCC -> supply required by your sensor
 *    GND -> ESP32 GND (common ground)
 *    OUT -> GPIO 34
 *
 *  !! The ESP32 ADC pin must NEVER see more than 3.3 V. If your Hall
 *  !! sensor runs from 5 V and its output can go above about 3.1 V,
 *  !! add a voltage divider (for example 10k / 20k, as in the
 *  !! multi-sensor project) and multiply by HALL_DIVIDER_RATIO below.
 *
 *  LiDAR VL53L4CD (planned):
 *    SDA -> GPIO 21, SCL -> GPIO 22, VCC -> 3V3, GND -> GND
 *    (these pins are free in this design; I2C address is 0x29)
 *
 * ------------------------------------------------------------
 *  SERIAL OUTPUT (one line every 500 ms)
 * ------------------------------------------------------------
 *  PACKET | RTD1=.. C | RTD2=.. C | HALL_RAW=.. | HALL_V=.. V | LIDAR=.. mm
 *
 *  Error values:  RTD = -999.00   HALL_RAW = -1   HALL_V = -1.000
 *                 LIDAR = -1 (also shown until the LiDAR is implemented)
 * ============================================================
 */

#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "max31865.h"

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

/* ---------- RTD 1: SPI2 ---------- */
#define RTD1_SPI_HOST       SPI2_HOST
#define RTD1_SCK            18
#define RTD1_MOSI           23
#define RTD1_MISO           19
#define RTD1_CS             25

/* ---------- RTD 2: SPI3 ---------- */
#define RTD2_SPI_HOST       SPI3_HOST
#define RTD2_SCK            14
#define RTD2_MOSI           13
#define RTD2_MISO           27
#define RTD2_CS             26

/* ---------- RTD settings ---------- */
#define RTD_NOMINAL_OHMS    100.0f      /* PT100 */
#define RTD_REFERENCE_OHMS  430.0f      /* Check the resistor on your MAX31865 board
                                           (marking "431" = 430 ohm, used for PT100) */
#define RTD_WIRING          MAX31865_4WIRE   /* or MAX31865_2WIRE / MAX31865_3WIRE */
#define RTD_FILTER          MAX31865_FILTER_50HZ  /* use _60HZ if your mains is 60 Hz */
#define SPI_CLOCK_HZ        1000000     /* 1 MHz (MAX31865 supports up to 5 MHz) */

/* ---------- Hall sensor ---------- */
#define HALL_ADC_GPIO       34
#define HALL_CHANNEL        ADC_CHANNEL_6
#define HALL_SAMPLES        16          /* readings averaged per measurement */
#define HALL_DIVIDER_RATIO  1.0f        /* 1.0 = no divider. Use 1.5 for a 10k/20k divider */

/* ---------- General ---------- */
#define LOOP_PERIOD_MS      500

/* ============================================================
 * GLOBALS
 * ============================================================ */

static const char *TAG = "SLAVE4";

static max31865_t rtd1;
static max31865_t rtd2;
static bool rtd1_ok = false;
static bool rtd2_ok = false;

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t         adc_cali_handle;
static bool                      adc_calibrated = false;
static bool                      hall_ok = false;

/* ============================================================
 * SPI + RTD SETUP
 * ============================================================ */

/* Starts one independent SPI bus. */
static esp_err_t init_spi_bus(spi_host_device_t host,
                              int sck_pin, int mosi_pin, int miso_pin)
{
    spi_bus_config_t bus_config = {
        .mosi_io_num = mosi_pin,
        .miso_io_num = miso_pin,
        .sclk_io_num = sck_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16,
    };

    return spi_bus_initialize(host, &bus_config, SPI_DMA_DISABLED);
}

/* Registers one MAX31865 on an already-started SPI bus and configures it. */
static esp_err_t init_rtd(max31865_t *dev, spi_host_device_t host,
                          int cs_pin, const char *name)
{
    esp_err_t err = max31865_init_desc(dev, host, SPI_CLOCK_HZ, cs_pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: MAX31865 init failed (%s)", name, esp_err_to_name(err));
        return err;
    }

    /* These must be set after init_desc */
    dev->rtd_nominal = RTD_NOMINAL_OHMS;
    dev->r_ref       = RTD_REFERENCE_OHMS;
    dev->standard    = MAX31865_ITS90;

    max31865_config_t config = {
        .v_bias     = true,
        .connection = RTD_WIRING,
        .mode       = MAX31865_MODE_SINGLE,
        .filter     = RTD_FILTER,
    };

    err = max31865_set_config(dev, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: configuration failed (%s)", name, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "%s ready", name);
    return ESP_OK;
}

static void init_rtds(void)
{
    /* ----- RTD 1 on SPI2 ----- */
    esp_err_t err = init_spi_bus(RTD1_SPI_HOST, RTD1_SCK, RTD1_MOSI, RTD1_MISO);
    if (err == ESP_OK) {
        rtd1_ok = (init_rtd(&rtd1, RTD1_SPI_HOST, RTD1_CS, "RTD 1") == ESP_OK);
    } else {
        ESP_LOGE(TAG, "RTD 1: SPI bus init failed (%s)", esp_err_to_name(err));
    }

    /* ----- RTD 2 on SPI3 ----- */
    err = init_spi_bus(RTD2_SPI_HOST, RTD2_SCK, RTD2_MOSI, RTD2_MISO);
    if (err == ESP_OK) {
        rtd2_ok = (init_rtd(&rtd2, RTD2_SPI_HOST, RTD2_CS, "RTD 2") == ESP_OK);
    } else {
        ESP_LOGE(TAG, "RTD 2: SPI bus init failed (%s)", esp_err_to_name(err));
    }
}

/* ============================================================
 * HALL SENSOR (ADC)
 * ============================================================ */

static esp_err_t init_hall(void)
{
    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_config, &adc_handle);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    err = adc_oneshot_config_channel(adc_handle, HALL_CHANNEL, &channel_config);
    if (err != ESP_OK) {
        return err;
    }

    /* Factory calibration improves voltage accuracy. */
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle) == ESP_OK) {
        adc_calibrated = true;
        ESP_LOGI(TAG, "Hall ADC calibration enabled");
    } else {
        ESP_LOGW(TAG, "Hall ADC calibration unavailable, using raw conversion");
    }

    return ESP_OK;
}

/* Averages several readings. Returns the raw count and the voltage at the
 * sensor output (divider ratio already applied). */
static bool hall_read(int *raw_avg, float *volts)
{
    int sum = 0;

    for (int i = 0; i < HALL_SAMPLES; i++) {
        int raw;
        if (adc_oneshot_read(adc_handle, HALL_CHANNEL, &raw) != ESP_OK) {
            return false;
        }
        sum += raw;
    }
    *raw_avg = sum / HALL_SAMPLES;

    float adc_voltage;
    int mv = 0;
    if (adc_calibrated &&
        adc_cali_raw_to_voltage(adc_cali_handle, *raw_avg, &mv) == ESP_OK) {
        adc_voltage = mv / 1000.0f;
    } else {
        adc_voltage = ((float)*raw_avg / 4095.0f) * 3.3f;
    }

    *volts = adc_voltage * HALL_DIVIDER_RATIO;
    return true;
}

/* ============================================================
 * LIDAR VL53L4CD (PLACEHOLDER)
 *
 * TO ADD THE LIDAR:
 *   1. Add a VL53L4CD driver to the project (ST's ultra-lite driver
 *      or an ESP-IDF component for the VL53L4CD).
 *   2. Create an I2C bus on SDA = GPIO 21, SCL = GPIO 22 and add the
 *      device at address 0x29.
 *   3. Put the driver's start-ranging call in lidar_init().
 *   4. In lidar_read_distance(), check the data-ready flag, read the
 *      distance, clear the interrupt, and return the value in mm.
 *
 * Until then this returns -1 so the packet format stays the same.
 * ============================================================ */

static void lidar_init(void)
{
    /* TODO: initialise VL53L4CD and start ranging */
}

static int lidar_read_distance(void)
{
    /* TODO: return distance in mm, or -1 on error */
    return -1;
}

/* ============================================================
 * MAIN
 * ============================================================ */

void app_main(void)
{
    printf("\n============================================================\n");
    printf("                    ESP32 SLAVE 4\n");
    printf("============================================================\n\n");
    printf("RTD 1 (SPI2): SCK=%d MOSI=%d MISO=%d CS=%d\n",
           RTD1_SCK, RTD1_MOSI, RTD1_MISO, RTD1_CS);
    printf("RTD 2 (SPI3): SCK=%d MOSI=%d MISO=%d CS=%d\n",
           RTD2_SCK, RTD2_MOSI, RTD2_MISO, RTD2_CS);
    printf("Hall ADC    : GPIO%d (ADC1_CH6)\n\n", HALL_ADC_GPIO);

    init_rtds();

    esp_err_t hall_err = init_hall();
    hall_ok = (hall_err == ESP_OK);
    if (!hall_ok) {
        ESP_LOGE(TAG, "Hall setup failed: %s", esp_err_to_name(hall_err));
    }

    lidar_init();

    printf("\n============================================================\n");
    printf("                      SENSOR PACKETS\n");
    printf("============================================================\n");

    while (1) {
        float temp1 = 0.0f;
        float temp2 = 0.0f;
        int   hall_raw = 0;
        float hall_volts = 0.0f;

        bool t1_valid = rtd1_ok && (max31865_measure(&rtd1, &temp1) == ESP_OK);
        bool t2_valid = rtd2_ok && (max31865_measure(&rtd2, &temp2) == ESP_OK);
        bool h_valid  = hall_ok && hall_read(&hall_raw, &hall_volts);

        int distance_mm = lidar_read_distance();

        printf("PACKET | "
               "RTD1=%.2f C | RTD2=%.2f C | "
               "HALL_RAW=%d | HALL_V=%.3f V | "
               "LIDAR=%d mm\n",
               t1_valid ? temp1 : -999.0f,
               t2_valid ? temp2 : -999.0f,
               h_valid ? hall_raw : -1,
               h_valid ? hall_volts : -1.0f,
               distance_mm);

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}