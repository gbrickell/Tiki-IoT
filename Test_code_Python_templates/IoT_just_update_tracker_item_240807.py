#!/usr/bin/python
# version: 240807
# python code template to be configured as required
# file name: IoT_just_update_tracker_item_240807.py - updates the field data of an existing tracker item
# uses the control_iot_240807.c and control_iot_240807.h functions compiled as a shared library libcontrol_iot_240807.so
# Author : Geoff Brickell
# Date   : 240807
# command to run in a CLI window - adjust the file path to suit your local device system: 
#    sudo python3 /your_file_path/IoT_just_update_tracker_item_240807.py
#  - run the command from the device CLI window to 'see' all the various responses/outputs from the Python and 'C' code
#
# In the code/comments below YYMMDD is used to signify version control/release 
#  and should be substituted for the versions being used e.g. 240807

# *****************
# *** IMPORTANT *** 
# This code, whilst it has undergone significant testing should be considered as early development 'quality' 
# and users should carry out their own testing/quality checks when incorporating it in their own system developments.
# The software is made available on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
# *****************


########################################################
####            various python functions            ####
####                but not all used!               ####
########################################################


########################################################
####                   main code                    ####
########################################################

import time

global debug
debug = 1   # set to 1 to see lots of additional output!!

# set up the use of the 'shared' library libcontrol_iot_YYMMDD.so of compiled C functions 
# that do various 'things' - compiled C is used to (hopefully!) provide something  
# common that can be used in different IoT contexts 
#  The .so library is created with the following gcc compiler command (where folder paths will have to be adjusted!):
#  gcc -shared -o /your_file_path/libcontrol_iot_YYMMDD.so -fPIC /your_file_path/control_iot_YYMMDD.c -I/usr/local/include -L/usr/local/lib -lcurl
# The following 'install' may be needed on a RPi:
# sudo apt install curl                       (this is usually already installed)
# sudo apt-get install libcurl4-openssl-dev   (this is more likely to be needed to avoid a gcc fatal error from curl/curl.h not being found)


import ctypes    # use ctypes so that the C code can be called from python
# libcontrol_iot_YYMMDD.so compiled using control_iot_YYMMDD.c and control_iot_YYMMDD.h
pi_iot_control_YYMMDD = ctypes.CDLL("/your_file_path/libcontrol_iot_YYMMDD.so")

#############################################################################
#call the IoT library 'connect' function to check the 'connection' to the   #
#compiled 'C' library - it should just display a simple 'hello' set of text #
#############################################################################
pi_iot_control_YYMMDD.connect_iot() 


##########################################
#  initialise all the various parameters #
##########################################

# define the various strings suitably for C usage

domain = "https://example_domain.com"      # must include https:// but no trailing /
b_domain = domain.encode('utf-8')

# include 'Authorization: Bearer' ahead of the token text as shown below
access_token = "Authorization: Bearer your_unique_security_access_token"    # Tiki API token for a specific Tiki user
b_access_token = access_token.encode('utf-8')

trackerId = "1"     # update to your tracker#
b_trackerId = trackerId.encode('utf-8')

itemIdget = "27"    # update to your tracker item#
b_itemIdget = itemIdget.encode('utf-8')

# this section of code is just to get a current time for one of the fields to be updated
now = time.strftime("%d %b %Y %H:%M:%S")   # this creates a string in a designated format e.g. 27 Dec 2021 11:05:27
nowepoch = round(time.time())              # BUT MUST USE the raw epoch integer to upload to a tracker to avoid time zone confusions!!

# update tracker item post data
#  this is just an example where the permanent field names (e.g. IoTtestDeviceName) are all individually given new values
update_itemdata =  "fields[IoTtestDeviceName]=rpi5-01c&fields[IoTtestUser]=rpi5-01&fields[IoTtestTextData]=python updated text&fields[IoTtestNumericalData]=789.0456&fields[IoTtestDateTimeData]=" + str(nowepoch) + "&fields[IoTtestTextAreaData]='C' code for no apostrophies around itemId\r\nupdated text data line 1\r\nupdated text data line 2\r\n&fields[IoTtestImage]=2&fields[IoTtestDescription]=updated item data sent from the rpi05-01 device using C code called from a python program"
b_update_itemdata = update_itemdata.encode('utf-8')



#####################################################
# call the tracker_itemupdate C function, to update #
# an existing tracker item passing it correctly     #
# defined char variables using ctypes               #
#####################################################
pi_iot_control_YYMMDD.tracker_itemupdate.restype = ctypes.c_char_p # override the default return type (int)
response = str( pi_iot_control_YYMMDD.tracker_itemupdate(ctypes.c_int(debug), ctypes.c_char_p(b_domain), ctypes.c_char_p(b_access_token), ctypes.c_char_p(b_trackerId), ctypes.c_char_p(b_itemIdupdate), ctypes.c_char_p(b_update_itemdata) ) )

print ("\n*** tracker_itemupdate response: " )
print ( response )
print ("\n \n" )
