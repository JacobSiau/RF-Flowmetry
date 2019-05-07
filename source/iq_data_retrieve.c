// Source code/libraries Copyright (c) Acconeer AB, 2018-2019
// Written by Jacob Siau, 2019
// All rights reserved to Acconeer

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "acc_driver_hal.h"
#include "acc_log.h"
#include "acc_rss.h"
#include "acc_service.h"
#include "acc_service_iq.h"
#include "acc_sweep_configuration.h"
#include "acc_version.h"

static acc_service_status_t execute_iq_with_blocking_calls(acc_service_configuration_t iq_configuration);
static acc_hal_t hal;
FILE *file;

int main(int argc, char** argv)
{   
    char filename[1000] = "/home/pi/Documents/rpi_xc111/iq_data_files/iq_data_last.txt";
    file = fopen(filename, "w");

    float sweep_start_m = .1f;
    float sweep_length_m = 0.4f;
    float sensor_gain_scale_factor = 1.0f;

    float user_sweep_start_m;
    float user_sweep_length_m;
    float user_sensor_gain_scale_factor;

    if (argc == 4) {
        user_sweep_start_m = strtof(argv[1], NULL);
        user_sweep_length_m = strtof(argv[2], NULL);
        user_sensor_gain_scale_factor = strtof(argv[3], NULL);

        if (user_sweep_start_m >= 0.1f && user_sweep_start_m <= 0.5f) {
            sweep_start_m = user_sweep_start_m;
        }

        if (user_sweep_length_m >= 0.1f && user_sweep_length_m <= 1.0f - user_sweep_start_m) {
            sweep_length_m = user_sweep_length_m;
        }

        if (user_sensor_gain_scale_factor >= 0.1f && user_sensor_gain_scale_factor <= 1.0f) {
            sensor_gain_scale_factor = user_sensor_gain_scale_factor;
        }
    }

    printf("[Sweep Start] %u mm \n", (unsigned int) (sweep_start_m * 1000.0f));
    printf("[Sweep Length] %u mm \n", (unsigned int) (sweep_length_m * 1000.0f));
    printf("[Sensor Gain Factor] %u %% \n", (unsigned int) (sensor_gain_scale_factor * 100.0f));

	if (!acc_driver_hal_init())
	{
		return EXIT_FAILURE;
	}

	hal = acc_driver_hal_get_implementation();

	if (!acc_rss_activate_with_hal(&hal))
	{
		return EXIT_FAILURE;
	}

    // create the iq configuration container
	acc_service_configuration_t iq_configuration = acc_service_iq_configuration_create();

	if (iq_configuration == NULL)
	{
		printf("[ERROR] acc_service_iq_configuration_create() failed\n");
		return EXIT_FAILURE;
	}

    // create a sweep configuration container with the iq configuration
    acc_sweep_configuration_t sweep_configuration = acc_service_get_sweep_configuration(iq_configuration);

    if (sweep_configuration == NULL) 
    {
        printf("[ERROR] acc_service_get_sweep_configuration() failed\n");
		return EXIT_FAILURE;
    }

    // set sweep parameters 
    acc_sweep_configuration_requested_range_set(sweep_configuration, sweep_start_m, sweep_length_m);
    float current_gain;
    current_gain = acc_sweep_configuration_receiver_gain_get(sweep_configuration);
    acc_sweep_configuration_receiver_gain_set(sweep_configuration, sensor_gain_scale_factor * current_gain);


	acc_service_status_t service_status;

	service_status = execute_iq_with_blocking_calls(iq_configuration);

	if (service_status != ACC_SERVICE_STATUS_OK)
	{
		printf("execute_iq_with_blocking_calls() => (%u) %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
		return EXIT_FAILURE;
	}

	acc_service_iq_configuration_destroy(&iq_configuration);
	acc_rss_deactivate();
    fclose(file);

	return EXIT_SUCCESS;
}


acc_service_status_t execute_iq_with_blocking_calls(acc_service_configuration_t iq_configuration)
{
	acc_service_handle_t handle = acc_service_create(iq_configuration);

	if (handle == NULL)
	{
		printf("[ERROR] acc_service_create failed\n");
		return ACC_SERVICE_STATUS_FAILURE_UNSPECIFIED;
	}

	acc_service_iq_metadata_t iq_metadata;
	acc_service_iq_get_metadata(handle, &iq_metadata);

	printf("Free space absolute offset: %u mm\n", (unsigned int)(iq_metadata.free_space_absolute_offset * 1000.0f));
	printf("Actual start: %u mm\n", (unsigned int)(iq_metadata.actual_start_m * 1000.0f));
	printf("Actual length: %u mm\n", (unsigned int)(iq_metadata.actual_length_m * 1000.0f));
	printf("Actual end: %u mm\n", (unsigned int)((iq_metadata.actual_start_m + iq_metadata.actual_length_m) * 1000.0f));
	printf("Data length: %u\n", (unsigned int)(iq_metadata.data_length));

	float complex                iq_data[iq_metadata.data_length];
	acc_service_iq_result_info_t result_info;

	acc_service_status_t service_status = acc_service_activate(handle);

	if (service_status == ACC_SERVICE_STATUS_OK)
	{
		uint_fast8_t sweep_count = 5;

		while (sweep_count > 0)
		{
			service_status = acc_service_iq_get_next(handle, iq_data, iq_metadata.data_length, &result_info);

			if (service_status == ACC_SERVICE_STATUS_OK)
			{
				//printf("IQ result_info->sequence_number: %u\n", (unsigned int)result_info.sequence_number);
				//printf("IQ data in polar coordinates (r, phi):\n");
				for (uint_fast16_t index = 0; index < iq_metadata.data_length; index++)
				{
					if (index && !(index % 8))
					{
						//printf("\n");
					}

					// printf("(%" PRIfloat ", %" PRIfloat ")\t", ACC_LOG_FLOAT_TO_INTEGER(cabsf(
					// 									    iq_data[index])),
					//        ACC_LOG_FLOAT_TO_INTEGER(cargf(iq_data[index])));
                    fprintf(file, "%" PRIfloat ", %" PRIfloat "\n", ACC_LOG_FLOAT_TO_INTEGER(crealf(iq_data[index])),
			            ACC_LOG_FLOAT_TO_INTEGER(cimagf(iq_data[index])));
				}

				//printf("\n");
			}
			else
			{
				printf("[ERROR] IQ data not properly retrieved\n");
			}

			sweep_count--;
		}

		service_status = acc_service_deactivate(handle);
	}
	else
	{
		printf("acc_service_activate() %u => %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
	}

	acc_service_destroy(&handle);

	return service_status;
}