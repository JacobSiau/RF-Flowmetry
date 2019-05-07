// Copyright (c) Acconeer AB, 2017-2019
// All rights reserved

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "acc_board.h"
#include "acc_definitions.h"
#include "acc_device_gpio.h"
#include "acc_device_os.h"
#include "acc_device_spi.h"
#include "acc_driver_gpio_linux_sysfs.h"
#include "acc_driver_os_linux.h"
#include "acc_driver_spi_linux_spidev.h"
#include "acc_log.h"

/**
 * @brief The module name
 *
 * Must exist if acc_log.h is used.
 */
#define MODULE "board_rpi_xc111_r4a_xr111-3_r1c"

/**
 * @brief The number of sensors available on the board
 */
#define SENSOR_COUNT 4

/**
 * @brief Host GPIO pin number (BCM)
 *
 * This GPIO should be connected to sensor 1 GPIO 5
 */
#define GPIO0_PIN 20

/**
 * @brief Host GPIO pin number (BCM)
 *
 * This GPIO should be connected to sensor 2 GPIO 5
 */
#define GPIO1_PIN 21

/**
 * @brief Host GPIO pin number (BCM)
 *
 * This GPIO should be connected to sensor 3 GPIO 5
 */
#define GPIO2_PIN 24

/**
 * @brief Host GPIO pin number (BCM)
 *
 * This GPIO should be connected to sensor 4 GPIO 5
 */
#define GPIO3_PIN 25

/**
 * @brief Host GPIO pin number (BCM)
 */
/**@{*/
#define RSTn_PIN   6
#define ENABLE_PIN 27

#define PMU_ENABLE_PIN 12
#define GLOREF_EN_PIN  13

#define CE_A_PIN 16
#define CE_B_PIN 19
/**@}*/

/**
 * @brief The reference frequency used by this board
 *
 * This assumes 24 MHz on XR111-3 R1C
 */
#define ACC_BOARD_REF_FREQ 24000000

/**
 * @brief The SPI speed of this board
 */
#define ACC_BOARD_SPI_SPEED 15000000

/**
 * @brief The SPI bus all sensors are using
 */
#define ACC_BOARD_BUS 0

/**
 * @brief The SPI CS all sensors are using
 */
#define ACC_BOARD_CS 0

/**
 * @brief Number of GPIO pins
 */
#define GPIO_PIN_COUNT 28

/**
 * @brief Sensor states
 */
typedef enum
{
	SENSOR_STATE_UNKNOWN,
	SENSOR_STATE_READY,
	SENSOR_STATE_BUSY
} acc_board_sensor_state_t;

/**
 * @brief Sensor state collection that keeps track of each sensor's current state
 */
static acc_board_sensor_state_t sensor_state[SENSOR_COUNT] = {SENSOR_STATE_UNKNOWN};

static const uint_fast8_t sensor_interrupt_pins[SENSOR_COUNT] = {
	GPIO0_PIN,
	GPIO1_PIN,
	GPIO2_PIN,
	GPIO3_PIN
};

static acc_board_isr_t     master_isr;
static acc_device_handle_t spi_handle;
static gpio_t              gpios[GPIO_PIN_COUNT];


static void isr_sensor1(void)
{
	if (master_isr)
	{
		master_isr(1);
	}
}


static void isr_sensor2(void)
{
	if (master_isr)
	{
		master_isr(2);
	}
}


static void isr_sensor3(void)
{
	if (master_isr)
	{
		master_isr(3);
	}
}


static void isr_sensor4(void)
{
	if (master_isr)
	{
		master_isr(4);
	}
}


/**
 * @brief Get the combined status of all sensors
 *
 * @return False if any sensor is busy
 */
static bool acc_board_all_sensors_inactive(void)
{
	for (uint_fast8_t sensor_index = 0; sensor_index < SENSOR_COUNT; sensor_index++)
	{
		if (sensor_state[sensor_index] == SENSOR_STATE_BUSY)
		{
			return false;
		}
	}

	return true;
}


bool acc_board_gpio_init(void)
{
	static bool           init_done  = false;

	if (init_done)
	{
		return true;
	}

	acc_os_init();

	if (
		!acc_device_gpio_set_initial_pull(GPIO0_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(GPIO1_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(GPIO2_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(GPIO3_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(RSTn_PIN, 1) ||
		!acc_device_gpio_set_initial_pull(ENABLE_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(PMU_ENABLE_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(GLOREF_EN_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(CE_A_PIN, 0) ||
		!acc_device_gpio_set_initial_pull(CE_B_PIN, 0))
	{
		ACC_LOG_WARNING("%s: failed to set initial pull", __func__);
	}

	if (
		!acc_device_gpio_input(GPIO0_PIN) ||
		!acc_device_gpio_input(GPIO1_PIN) ||
		!acc_device_gpio_input(GPIO2_PIN) ||
		!acc_device_gpio_input(GPIO3_PIN) ||
		!acc_device_gpio_write(RSTn_PIN, 0) ||
		!acc_device_gpio_write(ENABLE_PIN, 0) ||
		!acc_device_gpio_write(PMU_ENABLE_PIN, 0) ||
		!acc_device_gpio_write(GLOREF_EN_PIN, 0) ||
		!acc_device_gpio_write(CE_A_PIN, 0) ||
		!acc_device_gpio_write(CE_B_PIN, 0))
	{
		return false;
	}

	init_done = true;

	return true;
}


bool acc_board_init(void)
{
	static bool           init_done  = false;

	if (init_done)
	{
		return true;
	}

	acc_driver_os_linux_register();
	acc_os_init();

	acc_driver_gpio_linux_sysfs_register(GPIO_PIN_COUNT, gpios);
	acc_driver_spi_linux_spidev_register();

	acc_device_gpio_init();

	acc_device_spi_configuration_t configuration;

	configuration.bus           = ACC_BOARD_BUS;
	configuration.configuration = NULL;
	configuration.device        = ACC_BOARD_CS;
	configuration.master        = true;
	configuration.speed         = ACC_BOARD_SPI_SPEED;

	spi_handle = acc_device_spi_create(&configuration);

	for (uint_fast8_t sensor_index = 0; sensor_index < SENSOR_COUNT; sensor_index++)
	{
		sensor_state[sensor_index] = SENSOR_STATE_UNKNOWN;
	}

	init_done = true;

	return true;
}


/**
 * @brief Reset sensor
 *
 * Default setup when sensor is not active
 *
 * @return True if successful, false otherwise
 */
static bool acc_board_reset_sensor(void)
{
	if (!acc_device_gpio_write(RSTn_PIN, 0))
	{
		ACC_LOG_ERROR("Unable to activate RSTn");
		return false;
	}

	if (!acc_device_gpio_write(ENABLE_PIN, 0))
	{
		ACC_LOG_ERROR("Unable to deactivate ENABLE");
		return false;
	}

	if (!acc_device_gpio_write(PMU_ENABLE_PIN, 0))
	{
		ACC_LOG_ERROR("Unable to deactivate PMU_ENABLE");
		return false;
	}

	return true;
}


void acc_board_start_sensor(acc_sensor_id_t sensor)
{
	if (sensor_state[sensor - 1] == SENSOR_STATE_BUSY)
	{
		ACC_LOG_ERROR("Sensor %u already active.", sensor);
		return;
	}

	if (acc_board_all_sensors_inactive())
	{
		if (!acc_device_gpio_write(RSTn_PIN, 0))
		{
			ACC_LOG_ERROR("Unable to activate RSTn");
			acc_board_reset_sensor();
			return;
		}

		if (!acc_device_gpio_write(PMU_ENABLE_PIN, 1))
		{
			ACC_LOG_ERROR("Unable to activate PMU_ENABLE");
			acc_board_reset_sensor();
			return;
		}

		// Wait for PMU to stabilize
		acc_os_sleep_us(5000);

		if (!acc_device_gpio_write(ENABLE_PIN, 1))
		{
			ACC_LOG_ERROR("Unable to activate ENABLE");
			acc_board_reset_sensor();
			return;
		}

		// Wait for Power On Reset
		acc_os_sleep_us(5000);

		if (!acc_device_gpio_write(RSTn_PIN, 1))
		{
			ACC_LOG_ERROR("Unable to deactivate RSTn");
			acc_board_reset_sensor();
			return;
		}

		for (uint_fast8_t sensor_index = 0; sensor_index < SENSOR_COUNT; sensor_index++)
		{
			sensor_state[sensor_index] = SENSOR_STATE_READY;
		}
	}

	if (sensor_state[sensor - 1] != SENSOR_STATE_READY)
	{
		ACC_LOG_ERROR("Sensor has not been reset");
		return;
	}

	sensor_state[sensor - 1] = SENSOR_STATE_BUSY;
}


void acc_board_stop_sensor(acc_sensor_id_t sensor)
{
	if (sensor_state[sensor - 1] != SENSOR_STATE_BUSY)
	{
		ACC_LOG_ERROR("Sensor %u already inactive.", sensor);
		return;
	}

	sensor_state[sensor - 1] = SENSOR_STATE_UNKNOWN;

	if (acc_board_all_sensors_inactive())
	{
		acc_board_reset_sensor();
	}
}


bool acc_board_chip_select(acc_sensor_id_t sensor, uint_fast8_t cs_assert)
{
	if (cs_assert)
	{
		uint_fast8_t cea_val = (sensor == 1 || sensor == 2) ? 0 : 1;
		uint_fast8_t ceb_val = (sensor == 1 || sensor == 3) ? 0 : 1;

		if (
			!acc_device_gpio_write(CE_A_PIN, cea_val) ||
			!acc_device_gpio_write(CE_B_PIN, ceb_val))
		{
			ACC_LOG_ERROR("%s failed", __func__);
			return false;
		}
	}

	return true;
}


uint32_t acc_board_get_sensor_count(void)
{
	return SENSOR_COUNT;
}


bool acc_board_is_sensor_interrupt_connected(acc_sensor_id_t sensor)
{
	ACC_UNUSED(sensor);

	return true;
}


bool acc_board_is_sensor_interrupt_active(acc_sensor_id_t sensor)
{
	uint_fast8_t value;

	if (!acc_device_gpio_read(sensor_interrupt_pins[sensor - 1], &value))
	{
		ACC_LOG_ERROR("Could not obtain GPIO interrupt value for sensor %" PRIsensor "", sensor);
		return false;
	}

	return value != 0;
}


acc_integration_register_isr_status_t acc_board_register_isr(acc_board_isr_t isr)
{
	if (isr != NULL)
	{
		if (
			!acc_device_gpio_register_isr(GPIO0_PIN, ACC_DEVICE_GPIO_EDGE_RISING, &isr_sensor1) ||
			!acc_device_gpio_register_isr(GPIO1_PIN, ACC_DEVICE_GPIO_EDGE_RISING, &isr_sensor2) ||
			!acc_device_gpio_register_isr(GPIO2_PIN, ACC_DEVICE_GPIO_EDGE_RISING, &isr_sensor3) ||
			!acc_device_gpio_register_isr(GPIO3_PIN, ACC_DEVICE_GPIO_EDGE_RISING, &isr_sensor4))
		{
			return ACC_INTEGRATION_REGISTER_ISR_STATUS_FAILURE;
		}
	}
	else
	{
		if (
			!acc_device_gpio_register_isr(GPIO0_PIN, ACC_DEVICE_GPIO_EDGE_NONE, NULL) ||
			!acc_device_gpio_register_isr(GPIO1_PIN, ACC_DEVICE_GPIO_EDGE_NONE, NULL) ||
			!acc_device_gpio_register_isr(GPIO2_PIN, ACC_DEVICE_GPIO_EDGE_NONE, NULL) ||
			!acc_device_gpio_register_isr(GPIO3_PIN, ACC_DEVICE_GPIO_EDGE_NONE, NULL))
		{
			return ACC_INTEGRATION_REGISTER_ISR_STATUS_FAILURE;
		}
	}

	master_isr = isr;

	return ACC_INTEGRATION_REGISTER_ISR_STATUS_OK;
}


float acc_board_get_ref_freq(void)
{
	return ACC_BOARD_REF_FREQ;
}


bool acc_board_set_ref_freq(float ref_freq)
{
	ACC_UNUSED(ref_freq);

	return false;
}


void acc_board_sensor_transfer(acc_sensor_id_t sensor_id, uint8_t *buffer, size_t buffer_length)
{
	uint_fast8_t bus = acc_device_spi_get_bus(spi_handle);

	acc_device_spi_lock(bus);

	if (!acc_board_chip_select(sensor_id, 1))
	{
		ACC_LOG_ERROR("%s failed", __func__);
		acc_device_spi_unlock(bus);
		return;
	}

	if (!acc_device_spi_transfer(spi_handle, buffer, buffer_length))
	{
		acc_device_spi_unlock(bus);
		return;
	}

	acc_board_chip_select(sensor_id, 0);

	acc_device_spi_unlock(bus);
}
