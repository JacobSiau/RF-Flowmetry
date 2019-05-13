# RF-Flowmetry
***
Welcome to the GitHub page for the RF-Flowmetry project.

### Hardware: 
* Raspberry Pi 3 B+ [Link](https://www.raspberrypi.org/products/raspberry-pi-3-model-b-plus/)
* Sparkfun A111 Pulsed Radar Breakout Board [Link](https://www.sparkfun.com/products/retired/14811) 

### Software:
* Acconeer EVK for XC111 (Copyright Acconeer AB, 2018-2019)
* Sparkfun SDK Add-on [Link](https://github.com/sparkfunX/A111_Pulsed_Radar_Breakout/tree/master/Software/sparkx-sdk-addon)

***
### Description:
The project description is as follows:
> A LabVIEW program will set the flow rate on a pump and call a shell script on the Raspberry Pi
> to run a data retrieval program for iq data. 

***
### Steps to run the project: 
1. Connect the Raspberry Pi to the PC's network
2. SSH into the Pi by running the command: `ssh pi@<ip_address_here>`
3. cd into the directory `/home/pi/Documents/rpi_xc111`
4. Call the shell script by running the command: `./iq_data_retrieve.sh`
5. The array of IQ data will be written to STDOUT, and will be of the form `x, iy` split by newlines

***
### Running the project with different sensor parameters: 
The A111 sensor can be configured with the following parameters:
* `sweep_start_m`: The start distance in meters for the scans
  * Range: 0.1 < x < 0.5
  * Default: 0.1 
* `sweep_length_m`: The length of the sweep in meters, beginning at the start distance 
  * Range: 0.1 < x < (1.0 - sweep_start_m)
  * Default: 0.4
* `sensor_gain_scale_factor`: The scale factor (%) to scale the sensor gain by
  * Range: 0.1 < x < 1.0
  * Default: 1.0
  
To run the program with custom parameters, **all three parameters** must be present.
If all three parameters are not present in the invocation, then the defaults will be used as stated above.

Run the program with custom parameters by calling the shell script with the parameters.
Here's an example of calling the script for a sensor sweep from 200 mm to 600 mm, with a gain of 80%:
`./iq_data_retrieve.sh 0.2 0.4 0.8`

***
### Making changes to the source files
If changes are made to the source .c files, please run the command `make` to build and link the updated files. 
