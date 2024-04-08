#!/usr/bin/python
# version: 240403 ###
# python code template to be configured as required
# file name: IoT_just_update_T_opcode_240403.py - demonstrates how opcodes from a tracker item could be used to remotely initiate 'operations' on the hub device
#  - extracts an opcode from a tracker item at the Tiki site
#  - then replaces the opcode with a new code/text and re-uploads the tracker item
# 
# uses the control_iot_240403.c and control_iot_240403.h functions compiled as a shared library libcontrol_iot_240403.so
# Author : Geoff Brickell
# Date   : 240403
# command to run in a CLI window - adjust the file path to suit your local device system: 
#    sudo python3 /your_file_path/IoT_just_update_T_opcode_240403.py
#  - run the command from the device CLI window to 'see' all the various responses/outputs from the Python and 'C' code
#
# In the code/comments below YYMMDD is used to signify version control/release 
#  and should be substituted for the versions being used e.g. 240403

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

trackerId = "2"                            # update to your tracker# where all the device opcode details are stored
b_trackerId = trackerId.encode('utf-8')

itemId = "31"                              # update to your tracker item# for the opcode for the specific device is stored
b_itemId = itemId.encode('utf-8')


#####################################################
# call the tracker_itemget C function, to get       #
# an existing tracker item passing it correctly     #
# defined char variables using ctypes               #
#####################################################
pi_iot_control_YYMMDD.tracker_itemget.restype = ctypes.c_char_p # override the default return type (int)
response = str( pi_iot_control_YYMMDD.tracker_itemget(ctypes.c_int(debug), ctypes.c_char_p(b_domain), ctypes.c_char_p(b_access_token), ctypes.c_char_p(b_trackerId), ctypes.c_char_p(b_itemId)  ) )
print ("\n*** tracker_itemget response: " )
response = response[2:-1]
print ( response )
if "no response" in response or "failed" in response :
    print("\n curl request failed or response was empty \n")
    current_opcode = "failed"
else:
    respdict = eval(response)     # response string should now have a dictionary-like format so create an actual dictionary!
    print ("\n \n" )
    for key, value in respdict.items():
        print (key, " - ", value)
        if key == "IoTopcode":   # extract the current opcode from the tracker field
            current_opcode = value

###########################################################
#  section to intepret the opcode and execute something   #
#  - this is where a set of defined opcodes could be used #
#    to initiate specific operations with (say) a 000000  #
#    code used to 'do nothing'
###########################################################

if current_opcode == "failed":
    print ("curl request failed - so doing nothing\n")

elif current_opcode == "000000":
    print ("opcode is 000000 - so doing nothing\n")

elif current_opcode == "1qw3rz":
    print ("opcode is 1qw3rz - so doing the following:\n")
    print (" - nothing coded yet!\n")

else:
    print ("opcode not recognised - doing nothing\n") 

#######################################################
#  now reset the opcode to 000000 so the operations   # 
#   are not repeated at the next cycle                #
#######################################################
if current_opcode == "failed":
    print ("curl request failed - so don't do an update\n")

elif current_opcode == "000000":
    print ("opcode is already 000000 - so no need to update it\n")

else:
    # new itemId opcode
    newcode = "000000"
    # update tracker item post data:
    update_itemdata =  "fields[IoTopcode]=" + str(newcode)
    b_update_itemdata = update_itemdata.encode('utf-8')


    #####################################################
    # call the tracker_itemupdate C function, to update #
    # an existing tracker item passing it correctly     #
    # defined char variables using ctypes               #
    #####################################################
    pi_iot_control_YYMMDD.tracker_itemupdate.restype = ctypes.c_char_p # override the default return type (int)
    response = str( pi_iot_control_YYMMDD.tracker_itemupdate(ctypes.c_int(debug), ctypes.c_char_p(b_domain), ctypes.c_char_p(b_access_token),  ctypes.c_char_p(b_trackerId), ctypes.c_char_p(b_itemId), ctypes.c_char_p(b_update_itemdata) ) )
    print ("\n*** tracker_itemupdate response: " )
    response = response[2:-1]
    print ( response )
    print ("\n" )

print ("\nEND OF PROGRAM\n")