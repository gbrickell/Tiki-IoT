#!/usr/bin/python
# version: 240403 ###
# python code template to be configured as required
# file name: IoT_just_update_file_240403.py - updates the details of an existing file in a File gallery
# uses the control_iot_240403.c and control_iot_240403.h functions compiled as a shared library libcontrol_iot_240403.so
# Author : Geoff Brickell
# Date   : 240403
# command to run in a CLI window - adjust the file path to suit your local device system: 
#    sudo python3 /your_file_path/IoT_just_update_file_240403.py
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

fileId = "37"                          # update to the fileId# of your file that is being changed
b_fileId = fileId.encode('utf-8')

filepath = "/your_path/filename.jpg"   # update to your path/name/type and path should include the first / character
                                       # if no change is made to the file then this parameter should be blank/empty
b_filepath = filepath.encode('utf-8')


filename = "image_name.jpg"            # update to the name and type of your image
                                       # if no change is made to the filename then this parameter should be blank/empty
b_filename = filename.encode('utf-8')

filetitle = "your image title for the File gallery entry"   # update to your title text
                                       # if no change is made to the file title then this parameter should be blank/empty
b_filetitle = filetitle.encode('utf-8')

filedesc = "your image description for the File gallery entry"   # update to your description text
                                       # if no change is made to the file description then this parameter should be blank/empty
b_filedesc = filedesc.encode('utf-8')


# blank/empty parameters that can be used if no change is to be made
filepathblank = ""   
b_filepathblank = filepathblank.encode('utf-8')

filenameblank = ""
b_filenameblank = filenameblank.encode('utf-8')

filetitleblank = ""
b_filetitleblank = filetitleblank.encode('utf-8')

filedescblank = ""
b_filedescblank = filedescblank.encode('utf-8')


####################################################
# call the gallery_fileupdate C function,          #
#  to update an existing file in a File gallery    #
#  this example makes no change to the description #
####################################################
print ("\n*** gallery_fileupdate: not updating description ***\n" )
pi_iot_control_YYMMDD.gallery_fileupdate.restype = ctypes.c_char_p # override the default return type (int)
response = str( pi_iot_control_YYMMDD.gallery_fileupdate(ctypes.c_int(debug), ctypes.c_char_p(b_domain), ctypes.c_char_p(b_access_token), ctypes.c_char_p(b_fileId), ctypes.c_char_p(b_filepath), ctypes.c_char_p(b_filename), ctypes.c_char_p(b_filetitle), ctypes.c_char_p(b_filedescblank) ) )

print ("\n*** gallery_fileupdate response:\n" )
response = response[2:-1]
print ( response )            # response string should now have a dictionary-like format
respdict = eval(response)     # so create an actual dictionary!
print ("\n" )
for key, value in respdict.items():   # print all the values separately
    print (key, ": ", value)
