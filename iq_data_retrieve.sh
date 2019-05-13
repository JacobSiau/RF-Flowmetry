#!/bin/bash
cd /home/pi/Documents/rpi_xc111/ 
./out/iq_data_retrieve &> /dev/null
IFS=$'\n' read -d '' -r -a lines < /home/pi/Documents/rpi_xc111/iq_data_files/iq_data_last.txt
printf '%s\n' "${lines[@]}"

