#!/usr/bin/python
# version: 240403 ###
# python code template to be configured as required
# file name: IoT_just_down_file_240403.py - just downloads a file from a File gallery defined by its Tiki fileId#
# uses the control_iot_240403.c and control_iot_240403.h functions compiled as a shared library libcontrol_iot_240403.so
# Author : Geoff Brickell
# Date   : 240403
# command to run in a CLI window - adjust the file path to suit your local device system: 
#    sudo python3 /your_file_path/IoT_just_down_file_240403.py
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

fileIddown = "19"            # update to your fileId
b_fileIddown = fileIddown.encode('utf-8')

filespath = "/your_path/your_folder/"   # update to your path/folder name and should include both the first and last / character
b_filespath = filespath.encode('utf-8')

downfilename = "yourfilename.png"       # update to your filename/image type
b_downfilename = downfilename.encode('utf-8')

emptyfilename = ""
b_emptyfilename = emptyfilename.encode('utf-8')

downheaderfilename = "testheaders.txt"    # used as a temporary location for header info
b_downheaderfilename = downheaderfilename.encode('utf-8')

downheaderfilename2 = "testheaders2.txt"  # used as a 2nd temporary location for header info
b_downheaderfilename2 = downheaderfilename2.encode('utf-8')


#####################################################
# call the gallery_filedownload C function, to get  #
# an existing file gallery file passing it an       #
# explicit file name                                #
#####################################################
pi_iot_control_YYMMDD.gallery_filedownload.restype = ctypes.c_char_p # override the default return type (int)
response = str( pi_iot_control_YYMMDD.gallery_filedownload(ctypes.c_int(debug), ctypes.c_char_p(b_domain), ctypes.c_char_p(b_access_token), ctypes.c_char_p(b_fileIddown), ctypes.c_char_p(b_filespath), ctypes.c_char_p(b_downfilename), ctypes.c_char_p(b_downheaderfilename)  ) )

print ("\n*** gallery_filedownload response: " )
print ( response ) 



#####################################################
# call the gallery_filedownload C function, to get  #
# an existing file gallery file passing it an       #
# empty file name to trigger the use of a temp name #
#####################################################
pi_iot_control_YYMMDD.gallery_filedownload.restype = ctypes.c_char_p # override the default return type (int)
response = str( pi_iot_control_YYMMDD.gallery_filedownload(ctypes.c_int(debug), ctypes.c_char_p(b_domain), ctypes.c_char_p(b_access_token), ctypes.c_char_p(b_fileIddown), ctypes.c_char_p(b_filespath), ctypes.c_char_p(b_emptyfilename), ctypes.c_char_p(b_downheaderfilename2)  ) )

print ("\n*** gallery_filedownload response: " )
print ( response )