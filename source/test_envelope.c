// Copyright (c) Acconeer AB, 2015-2019
// All rights reserved

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "acc_driver_hal.h"
#include "acc_rss.h"
#include "acc_service.h"
#include "acc_service_envelope.h"
#include "acc_sweep_configuration.h"

#include "acc_version.h"


/**
 * @brief Example that shows how to use the envelope service
 *
 * This is an example on how the envelope service can be used.
 * The example executes as follows:
 *   - Activate Radar System Services (RSS)
 *   - Create an envelope service configuration (with blocking mode as default)
 *   - Create an envelope service using the previously created configuration
 *   - Activate the envelope service
 *   - Get the result and print it 5 times, where the last result is intentionally late
 *   - Deactivate and destroy the envelope service
 *   - Reconfigure the envelope service configuration with callback mode
 *   - Create and activate envelope service
 *   - Get the result and print it 2 times
 *   - Deactivate and destroy the envelope service
 *   - Destroy the envelope service configuration
 *   - Deactivate Radar System Services
 */


typedef struct
{
	uint16_t     data_length;
	uint_fast8_t sweep_count;
} envelope_callback_user_data_t;

static acc_service_status_t execute_envelope_with_blocking_calls(acc_service_configuration_t envelope_configuration);
// static acc_service_status_t execute_envelope_with_callback(acc_service_configuration_t envelope_configuration);
// static void envelope_callback(const acc_service_handle_t service_handle, 
//                               const uint16_t *envelope_data,
//                               const acc_service_envelope_result_info_t *result_info, 
//                               void *user_reference);
// static void reconfigure_sweeps(acc_service_configuration_t envelope_configuration);
// static bool any_sweeps_left(const envelope_callback_user_data_t *callback_user_data);
// static acc_os_mutex_t callback_data_lock;

static acc_hal_t hal;

void formatAsTimeUTC(char *output);

FILE *file;

int main(void)
{
    char current_time[100];
    formatAsTimeUTC(current_time);
    printf("Current local time and date: %s", current_time);
    
    char filename[1000] = "/home/pi/Documents/rpi_xc111/test_envelope_data/";
    char filetail[5] = ".txt";
    char filetime[1500];
    formatAsTimeUTC(filetime);
    strcat(filename, filetime);
    strcat(filename, filetail);
    
    file = fopen(filename, "w");

    fprintf(file, "[ENVELOPE DATA TEST] Current local time and date: %s\n", current_time);

	printf("Acconeer software version %s\n", ACC_VERSION);
	printf("Acconeer RSS version %s\n", acc_rss_version());
    fprintf(file, "Acconeer software version %s\n", ACC_VERSION);
	fprintf(file, "Acconeer RSS version %s\n", acc_rss_version());

	if (!acc_driver_hal_init())
	{
		return EXIT_FAILURE;
	}

	hal = acc_driver_hal_get_implementation();

	if (!acc_rss_activate_with_hal(&hal))
	{
		return EXIT_FAILURE;
	}

	acc_service_configuration_t envelope_configuration = acc_service_envelope_configuration_create();

	if (envelope_configuration == NULL)
	{
		printf("[ERROR] acc_service_envelope_configuration_create() failed\n");
        fprintf(file, "[ERROR] acc_service_envelope_configuration_create() failed\n");
		return EXIT_FAILURE;
	}

	acc_service_status_t service_status;

	// optimize filter for static objects (from Acconeer's envelope user guide)
	acc_service_envelope_running_average_factor_set(envelope_configuration, 0.95);

	acc_sweep_configuration_t sweep_configuration;
	sweep_configuration = acc_service_get_sweep_configuration(envelope_configuration);

	if (sweep_configuration == NULL) 
	{
		printf("[ERROR] acc_service_get_sweep_configuration() failed\n");
		fprintf(file, "[ERROR] acc_service_get_sweep_configuration() failed\n");
		return EXIT_FAILURE;
	}

	// set sweep range, 100mm to 500mm
	acc_sweep_configuration_requested_range_set(sweep_configuration, .10, 0.4);

	// control receiver gain in the sensor
	float current_gain;
	current_gain = acc_sweep_configuration_receiver_gain_get(sweep_configuration);
	acc_sweep_configuration_receiver_gain_set(sweep_configuration, 0.8f * current_gain);

	// set the service status to the main function call
	service_status = execute_envelope_with_blocking_calls(envelope_configuration);

	if (service_status != ACC_SERVICE_STATUS_OK)
	{
		printf("execute_envelope_with_blocking_calls() => (%u) %s\n", (unsigned int)service_status, acc_service_status_name_get(
			       service_status));
        fprintf(file, "execute_envelope_with_blocking_calls() => (%u) %s\n", (unsigned int)service_status, acc_service_status_name_get(
        service_status));
		acc_service_envelope_configuration_destroy(&envelope_configuration);
		return EXIT_FAILURE;
	}

	// reconfigure_sweeps(envelope_configuration);

	// service_status = execute_envelope_with_callback(envelope_configuration);

	// if (service_status != ACC_SERVICE_STATUS_OK)
	// {
	// 	printf("execute_envelope_with_callback() => (%u) %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
	// 	fprintf(file, "execute_envelope_with_callback() => (%u) %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
    //     acc_service_envelope_configuration_destroy(&envelope_configuration);
	// 	acc_rss_deactivate();
	// 	return EXIT_FAILURE;
	// }

	acc_service_envelope_configuration_destroy(&envelope_configuration);
	acc_rss_deactivate();

    fclose(file);

	return EXIT_SUCCESS;
}


acc_service_status_t execute_envelope_with_blocking_calls(acc_service_configuration_t envelope_configuration)
{
	acc_service_handle_t handle = acc_service_create(envelope_configuration);

	if (handle == NULL)
	{
		printf("[ERROR] acc_service_create failed\n");
        fprintf(file, "[ERROR] acc_service_create failed\n");
		return ACC_SERVICE_STATUS_FAILURE_UNSPECIFIED;
	}

	acc_service_envelope_metadata_t envelope_metadata;
	acc_service_envelope_get_metadata(handle, &envelope_metadata);

	printf("Free space absolute offset: %u mm\n", (unsigned int)(envelope_metadata.free_space_absolute_offset * 1000.0f));
	printf("Actual start: %d mm\n", (int)(envelope_metadata.actual_start_m * 1000.0f));
	printf("Actual length: %u mm\n", (unsigned int)(envelope_metadata.actual_length_m * 1000.0f));
	printf("Actual end: %d mm\n", (int)((envelope_metadata.actual_start_m + envelope_metadata.actual_length_m) * 1000.0f));
	printf("Data length: %u\n", (unsigned int)(envelope_metadata.data_length));
    fprintf(file, "Free space absolute offset: %u mm\n", (unsigned int)(envelope_metadata.free_space_absolute_offset * 1000.0f));
	fprintf(file, "Actual start: %d mm\n", (int)(envelope_metadata.actual_start_m * 1000.0f));
	fprintf(file, "Actual length: %u mm\n", (unsigned int)(envelope_metadata.actual_length_m * 1000.0f));
	fprintf(file, "Actual end: %d mm\n", (int)((envelope_metadata.actual_start_m + envelope_metadata.actual_length_m) * 1000.0f));
	fprintf(file, "Data length: %u\n", (unsigned int)(envelope_metadata.data_length));

	uint16_t envelope_data[envelope_metadata.data_length];

	acc_service_envelope_result_info_t result_info;
	acc_service_status_t               service_status = acc_service_activate(handle);

	if (service_status == ACC_SERVICE_STATUS_OK)
	{
		uint_fast8_t sweep_count = 5;

		while (sweep_count > 0)
		{
			service_status = acc_service_envelope_get_next(handle, envelope_data, envelope_metadata.data_length, &result_info);

			if (service_status == ACC_SERVICE_STATUS_OK)
			{
				printf("Envelope result_info.sequence_number: %u\n", (unsigned int)result_info.sequence_number);
				printf("Envelope data:\n");
                fprintf(file, "Envelope result_info.sequence_number: %u\n", (unsigned int)result_info.sequence_number);
				fprintf(file, "Envelope data:\n");
				for (uint_fast16_t index = 0; index < envelope_metadata.data_length; index++)
				{
					// if (index && !(index % 8))
					// {
					// 	printf("\n");
                    //     fprintf(file, "\n");
					// }

					printf("%.6u\n", (unsigned int)(envelope_data[index]));
                    fprintf(file, "%.6u\n", (unsigned int)(envelope_data[index]));
				}

				printf("\n");
                fprintf(file, "\n");
			}
			else
			{
				printf("[ERROR] Envelope data not properly retrieved\n");
                fprintf(file, "[ERROR] Envelope data not properly retrieved\n");
			}

			sweep_count--;
		}

		service_status = acc_service_deactivate(handle);
	}
	else
	{
		printf("acc_service_activate() %u => %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
        fprintf(file, "acc_service_activate() %u => %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
    }

	acc_service_destroy(&handle);

	return service_status;
}


// acc_service_status_t execute_envelope_with_callback(acc_service_configuration_t envelope_configuration)
// {
// 	envelope_callback_user_data_t callback_user_data;

// 	acc_service_envelope_envelope_callback_set(envelope_configuration, &envelope_callback, &callback_user_data);

// 	callback_data_lock = hal.os.mutex_create();
// 	if (callback_data_lock == NULL)
// 	{
// 		printf("Failed to create mutex.\n");
// 		return EXIT_FAILURE;
// 	}

// 	acc_service_handle_t handle = acc_service_create(envelope_configuration);

// 	if (handle == NULL)
// 	{
// 		printf("acc_service_create failed\n");
// 		return ACC_SERVICE_STATUS_FAILURE_UNSPECIFIED;
// 	}

// 	acc_service_envelope_metadata_t envelope_metadata;
// 	acc_service_envelope_get_metadata(handle, &envelope_metadata);

// 	printf("Free space absolute offset: %u mm\n", (unsigned int)(envelope_metadata.free_space_absolute_offset * 1000.0f));
// 	printf("Actual start: %d mm\n", (int)(envelope_metadata.actual_start_m * 1000.0f));
// 	printf("Actual length: %u mm\n", (unsigned int)(envelope_metadata.actual_length_m * 1000.0f));
// 	printf("Actual end: %d mm\n", (int)((envelope_metadata.actual_start_m + envelope_metadata.actual_length_m) * 1000.0f));
// 	printf("Data length: %u\n", (unsigned int)(envelope_metadata.data_length));

// 	// Configure callback user data
// 	callback_user_data.sweep_count = 2;
// 	callback_user_data.data_length = envelope_metadata.data_length;

// 	acc_service_status_t service_status = acc_service_activate(handle);

// 	if (service_status == ACC_SERVICE_STATUS_OK)
// 	{
// 		while (any_sweeps_left(&callback_user_data))
// 		{
// 			hal.os.sleep_us(200);
// 		}

// 		service_status = acc_service_deactivate(handle);
// 	}
// 	else
// 	{
// 		printf("acc_service_activate() %u => %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
// 	}

// 	acc_service_destroy(&handle);

// 	hal.os.mutex_destroy(callback_data_lock);

// 	return service_status;
// }


// void envelope_callback(const acc_service_handle_t service_handle, const uint16_t *envelope_data,
//                        const acc_service_envelope_result_info_t *result_info, void *user_reference)
// {
// 	(void)service_handle;

// 	if (result_info->sensor_communication_error)
// 	{
// 		// Handle error, for example restart service
// 	}

// 	if (result_info->data_saturated)
// 	{
// 		// Handle warning, for example lowering the gain
// 	}

// 	envelope_callback_user_data_t *callback_user_data = user_reference;

// 	hal.os.mutex_lock(callback_data_lock);

// 	if (callback_user_data->sweep_count > 0)
// 	{
// 		printf("Envelope callback result_info->sequence_number: %u\n", (unsigned int)result_info->sequence_number);
// 		printf("Envelope callback data:\n");
// 		for (uint_fast16_t index = 0; index < callback_user_data->data_length; index++)
// 		{
// 			if (index && !(index % 8))
// 			{
// 				printf("\n");
// 			}

// 			printf("%6u", (unsigned int)(envelope_data[index]));
// 		}

// 		printf("\n");

// 		callback_user_data->sweep_count--;
// 	}

// 	hal.os.mutex_unlock(callback_data_lock);
// }


// static bool any_sweeps_left(const envelope_callback_user_data_t *callback_user_data)
// {
// 	hal.os.mutex_lock(callback_data_lock);
// 	bool sweeps_left = callback_user_data->sweep_count > 0;
// 	hal.os.mutex_unlock(callback_data_lock);

// 	return sweeps_left;
// }


// void reconfigure_sweeps(acc_service_configuration_t envelope_configuration)
// {
// 	acc_sweep_configuration_t sweep_configuration = acc_service_get_sweep_configuration(envelope_configuration);

// 	if (sweep_configuration == NULL)
// 	{
// 		printf("Sweep configuration not available\n");
// 	}
// 	else
// 	{
// 		float start_m            = 0.4f;
// 		float length_m           = 0.5f;
// 		float sweep_frequency_hz = 100;

// 		acc_sweep_configuration_requested_start_set(sweep_configuration, start_m);
// 		acc_sweep_configuration_requested_length_set(sweep_configuration, length_m);

// 		acc_sweep_configuration_repetition_mode_streaming_set(sweep_configuration, sweep_frequency_hz);
// 	}
// }

// Reference: https://stackoverflow.com/a/30521446
void formatAsTimeUTC(char *output)
{
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(
        output,
        "%d_%d_%d_%d_%d_%d_UTC",
        timeinfo->tm_mday,
        timeinfo->tm_mon+1,
        timeinfo->tm_year+1900,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec
        );
}
