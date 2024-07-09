#!/usr/bin/python
# TCP_socket_send01.py demonstration TCP socket data send routine, running
#  on a Sense Box system that collects data from two DS18B20 1-wire sensors
#  - see https://onlinedevices.org.uk/RPi+Maker+PCB+-+Sensor+box+project
#    for more details on a Sense Box

# used to show how data is sent to a remote TCP socket server

# Author : Geoff Brickell
# Date   : 240708

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
## function to check that the remote socket server is available
##+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
def checksockserver(ip):
    if pingtestip(ip, 3):    # single IP address ping test
        print ("socket server is available - so OK to continue")
        return True
    else:
        os.system("sudo ifconfig wlan0 down")
        time.sleep(5)
        os.system("sudo ifconfig wlan0 up")
        time.sleep(5)
        print("socket server wasn't available - so toggled the wlan0 interface down/up to see if that helps")
        if pingtest(node, 3):
            print ("socket server is available - doing nothing")
            return True
        else:
            print("socket server wasn't available - and toggling the wlan interface didn't help")
            return False


#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
## function to test ping a single IP address
##+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
def pingtestip (ipadr, ping_count):
    # ping_count is the number of test pings to send
    x = subprocess.run(["ping " + str(ipadr) + " -n -c " + str(ping_count)], shell=True, capture_output=True, text=True)
    ping_test = findall("ttl", x.stdout)  # look for a correct output from the ping
    if ping_test:
        print(f"{ipadr} : Successful Ping")
        return True
    else:
        print(f"{ipadr} : Failed Ping: more info below")
        print ("\n*** x.args:")
        print(x.args)
        print ("\n*** x.retrncode:")
        print(x.returncode)
        print ("\n*** x.stdout:")
        print(x.stdout)
        print ("\n*** x.stderr:")
        print(x.stderr)
        return False
		

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
## function to test and if necessary repower the 1-wire DS18B20 sensors
##+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

def one_wire_check(pow_pin, sen1, sen2):
    # check that both the 1-wire device files are present
    if (os.path.isdir("/sys/bus/w1/devices/" + sen1) == False) or (os.path.isdir("/sys/bus/w1/devices/" + sen2) == False):
        # either one or both files are missing so reset the interface
        print("resetting the 1-wire interface")
        GPIO.output(pow_pin, GPIO.LOW)
        time.sleep(3)
        GPIO.output(pow_pin, GPIO.HIGH)
        time.sleep(5)


#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
## function to just repower the 1-wire DS18B20 sensors
##+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

def one_wire_reset(pow_pin):
    # reset the interface
    print("resetting the 1-wire interface")
    GPIO.output(pow_pin, GPIO.LOW)
    time.sleep(3)
    GPIO.output(pow_pin, GPIO.HIGH)
    time.sleep(5)


#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
## separate function to read the raw temperature reading from the 
#   1-wire interface file automatically created for the detected device
#   where the device address is passed as a parameter
# - called from read_temp(sensor_address)
##+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
def temp_raw(address):
    f = open(address, 'r')
    lines = f.readlines()
    f.close()
    return lines

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
# function to process DS18B20 temperature reading
#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
def read_temp(sensor_address):
    # get a first set of raw temperature readings from the 1-wire interface data file
    lines = temp_raw(sensor_address)   
    #print (str(lines))
    #print ("lines length: " + str(len(lines)) )
    if len(lines) < 2:   # check that we have valid length data to avoid an IndexError below
        return 0, 0      # return zero values if lines is too short


    # now keep checking the 1-wire interface data file until new/complete data has been received
    while lines[0].strip()[-3:] != 'YES':    #loop until we find 'YES' on the 1st line
        time.sleep(0.2)
        lines = temp_raw()

    # if here then good data has been received from the 1-wire bus
    #    so decode and display the temperature reading in degC and degF
    temp_output = lines[1].find('t=')

    if temp_output != -1:
        temp_string = lines[1].strip()[temp_output+2:]
        temp_c = float(temp_string) / 1000.0
        temp_f = (temp_c * 9.0 / 5.0) + 32.0
        return temp_c, temp_f

    # return to the calling code to repeat the cycle


#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#
# main code
#
#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

import time
import socket
import sys
import os
import subprocess

from re import findall
from subprocess import Popen, PIPE

os.environ['TZ'] = 'Europe/London'
time.tzset()

# ********************************
# GPIO set up for DS18B20 powering
# ********************************
import RPi.GPIO as GPIO # import the GPIO pin management functions
# initialize GPIO
GPIO.setwarnings(False)  # suppress any GPIO pin warning outputs
GPIO.setmode(GPIO.BCM)   # use the standard BCM pin numbering
GPIO.cleanup()   # clean up in case anything may have been previously set and not cleared
# this GPIO# is specific to the Sense Box build
power_pin = 14  # GPIO pin used to power the DS18B20 sensors
GPIO.setup(power_pin, GPIO.OUT)
GPIO.output(power_pin, GPIO.HIGH)
time.sleep(5)
print ("pin #14 set high so the 1-wire interface should be OK")

# ************************
# DS18B20 set up
# ************************
# 1-wire interface assumed to be 'on' and the default GPIO#4 pin being used for the 1-wire data input

# set the temp sensors unique serial codes to that of the specific DS18B20 being used
# Sense Box build with two DS1820B metal sheathed test probes
# set thse values to the unique Ids of the DS18B20 sensors being used
temp_code1 = 'nn-nnnnnnnnnnnn'# (test probe label #1)
temp_code2 = 'nn-nnnnnnnnnnnn'# (test probe label #2)

# set the full path/file address for where the 1-wire interface puts the sensor's data
sensor_code1 = '/sys/bus/w1/devices/' + temp_code1 + '/w1_slave'
sensor_code2 = '/sys/bus/w1/devices/' + temp_code2 + '/w1_slave'

# check the device files are OK
one_wire_check(power_pin, temp_code1, temp_code2) 

# collect the current temperatures
tempc1, tempf1 = read_temp(sensor_code1) # reads and processes the #1 probe
tempc2, tempf2 = read_temp(sensor_code2) # reads and processes the #2 probe

if tempc1 == 0.0 and tempc2 == 0.0 :
    one_wire_reset(power_pin)
    tempc1, tempf1 = read_temp(sensor_code1) # re-reads and processes the #1 probe
    tempc2, tempf2 = read_temp(sensor_code2) # re-reads and processes the #2 probe

tempC1str = "%.2f" % tempc1
if tempc1 < -9.999:
    tempC1str = "%.1f" % tempc1
tempC1str = tempC1str.zfill(5)   # add a leading zero for temp less than 10.00 ie always make the string 5 characters
print(tempC1str + " degC - " + str(round(tempf1,3)) + " degF")

tempC2str = "%.2f" % tempc2
if tempc2 < -9.999:
    tempC2str = "%.1f" % tempc2
tempC2str = tempC2str.zfill(5)   # add a leading zero for temp less than 10.00 ie always make the string 5 characters
print(tempC2str + " degC - " + str(round(tempf2,3)) + " degF")

# socket send data to demonstration TCP socket server
PORT = 8888
SIZE = 1024
HOST = ''	# Symbolic name meaning all available interfaces on this device

# set the IP address of the remote TCP socket server here
SERVER_IP   = 'xxx.xxx.xxx.xxx' 

server_address = (SERVER_IP, PORT)

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
print ('Socket created')

# check that the remote server is 'there'
if checksockserver(SERVER_IP):
    # connect to the demonstration socket server and read welcome text
    conn_count = 0
    while conn_count < 10:
        try:
            s.connect(server_address)    # if an error occurs the 'except' waits then the 'while' tries again
            data = s.recv(1024)   # received bytes
            print ("data received: " + data.decode("utf-8") )
            if "Welcome to the demonstration TCP socket server" in data.decode("utf-8"):
                print ("Socket connected and the correct 'welcome' text received")
                break  # break out of while loop

        except:
            print (" can't connect to server - reconnecting, waiting and then trying again")
            time.sleep(10)
            conn_count = conn_count + 1
            if conn_count == 10:
                print (" couldn't connect to the demonstration socket server in time - aborting session")
                sys.exit()

            else:
                print ("conn_count = " + str(conn_count) + ": couldn't connect to the demonstration socket server\n - closing and reopening socket before trying again ")
                s.close()
                time.sleep(2)
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                print ('Socket re-created')
                time.sleep(4)


    # connection made so now send two sets of data in the designated format ie
    #  msg format is: xxxxxxxx - 'value as a string' :'epoch-integer as a string'END
    #  where xxxxxxxx is an 8 character label that indicates the data source
    nowepoch = round(time.time())
    datamsg1 = "sense001 - " + tempC1str + " :" + str(nowepoch) + "END"
    datamsg2 = "sense002 - " + tempC2str + " :" + str(nowepoch) + "END"

    # send first set of data
    send_tries = 1
    while send_tries <10:
        print ("send try: " + str(send_tries) )
        s.sendall(str.encode(datamsg1))
        # read reply
        data = s.recv(2048)   # received bytes with buffer size 2048
        data_decoded = data.decode("utf-8")
        print ("data received: " + data_decoded )
        send_tries = send_tries + 1
        if data_decoded[0:5] == "OK...":
            print ("DS18B20-1 temperature data socket sent OK")
            break
        if send_tries == 10:
            print ("DS18B20-1 temperature data socket send NOT OK - aborting session")

    # send second set of data
    send_tries = 1
    while send_tries <10:
        print ("send try: " + str(send_tries) )
        s.sendall(str.encode(datamsg2))
        # read reply
        data = s.recv(2048)   # received bytes with buffer size 2048
        data_decoded = data.decode("utf-8")
        print ("data received: " + data_decoded )
        send_tries = send_tries + 1
        if data_decoded[0:5] == "OK...":
            print ("DS18B20-2 temperature data socket sent OK")
            break
        if send_tries == 10:
            print ("DS18B20-2 temperature data socket send NOT OK - aborting session")

else:
    print ("couldn't find remote server - ending program")

print ("\n*****end of program ******\n\n" )
