# Arduino-Python-Code
Code for arduino and python controller.

# Code

## Main

This is code for the arduino.

## Controller

This is the code for the python controller

# Communication

The python module and arduino communicates through serial via usb connection. 
It requires this for it to work correctly, if not the commands arent recieved.

The python module communicates with a backend server, this is in the other repository on this account.

# Known issues

Python module doesnt always pick up results, this is most likely an issue of multithreading/concurrency as reading is done in a diffrent thread than the command requests.
