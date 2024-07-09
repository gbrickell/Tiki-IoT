#!/usr/bin/python
# TCP_socket_server01.py - demonstration TCP socket server that would run on an IoT local
#  hub device to collect data from satellites that use TCP socket to send data to the hub
# Author : Geoff Brickell
# Date   : 240708
# version 01

##############################################
##   functions
##############################################

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
# main TCP socket server handler run as a thread
#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

def socketthread(name): 
    global client1IP, client2IP
    HOST = ''	# Symbolic name meaning all available interfaces
    PORT = 8888	# Arbitrary non-privileged port

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    print ('Socket created')
    status = 'socket created'

    tries = 0
    #Bind socket to local host and port and use a while/try combo for upto 10 attempts
    while tries < 10:
        try:
            s.bind((HOST, PORT))
            break
        except socket.error as msg:
            print ('Try ' + str(tries) + ': bind failed. Error details : ' + str(msg) )
            tries = tries + 1
            if tries == 10:
                print ("too many attempts to do the socket 'bind' - so aborting the program")
                sys.exit()
	
    print ('Socket bind complete')

    #Start listening on socket
    s.listen(10)
    print ('Socket now listening .... \n')

    #now keep talking with individual 'clients'
    while 1:   # continuous loop 'detecting' new client 'inputs' to the server
        #wait to accept a connection - blocking call
        conn, addr = s.accept()
        print ('Connected with ' + addr[0] + ':' + str(addr[1]))
    
        #start new thread: 1st argument as a function name to be run, 
        #                  2nd is the tuple of arguments to the function
        #                  3rd sets daemon True so that thread stops immediately when the main 'thread' closes
        # extract key text from 'conn' to use as the thread name
        threadname = find_between( str(conn), "raddr=", ">" )
        print("starting new 'talking with client' thread " + str(conn) + "  .........")
        print(" just using " + threadname + " the text in the connection request, for the new thread name")
        clientIP = find_between(threadname, "'", "'" )
        print(" the client IP address is: " + clientIP )

        # check which IP address is 'sending' data
        if clientIP == client1IP:
            print("client device is client1 - a test TCP sending device")
        elif clientIP == client2IP:
            print("client device is client2 - a test TCP sending devicm")
        # add more possible client device checks here

        x = threading.Thread(target=clientthread, name=threadname, args=(conn,), daemon=True)  #
        threads.append(x) # update the list of opened threads
        x.start()

    print ("closing socket ....")   # should never really get here unless the while loop above breaks for some reason
    s.close()

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#  Functionality in a new thread for handling data from/to a new connection
#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
def clientthread(conn):

    #Sending response text message to the new connected client
    conn.send(str.encode('Welcome to the demonstration TCP socket server. Type something and hit enter\n')) # defined text that the client cross-checks!
	
    #infinite loop so that function does not terminate and thread does not end until no more data is received
    while True:
	
        #Receiving from client
        data = conn.recv(1024)   # received as bytes so encode so it can be more easily handled as a string
        reply = str.encode("OK..." + data.decode("utf-8") )

        if not data: 
            break

        print ("data received: sending reply " + "OK..." + data.decode("utf-8") ) 
        conn.sendall(reply)  # send ack to client - again defined text that the sending client can cross-check
        # decode received data string so input can be 'decoded' using the decode_data function below
        decode_data(data.decode("utf-8"))
        print ("data decoded: ")
	
    #came out of loop after a break when data stopped arriving
    print ("*** no more data - so closing connection and going back to listening ... ***\n")
    conn.close()


#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
# decode and display the received data string from the TCP socket client
#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
def decode_data(datastring):
    # to avoid a multiplicity of 'returned' variables a set of 'globals' are used for each
    #  of the expected data items that will be being sent to the TCP socket server
    global AQsys001, AQsys002, AQsys003, AQsys004, AQsys005  # received data from an AQM IoT system
    global sense001, sense002, sense003, sense004, sense005  # received data from a general Sense Box IoT system

    # initialise the incoming 'signifying' text strings that identify the data/source

    AQsys001 = "not set"  # air quality system: hallway air temperature
    AQsys002 = "not set"  # air quality system: hallway humidity
    AQsys003 = "not set"  # air quality system: hallway air pressure
    AQsys004 = "not set"  # air quality system: hallway air PM2.5
    AQsys005 = "not set"  # air quality system: hallway air PM10

    sense001 = "not set"  # Sense box: freezer-1 temperature
    sense002 = "not set"  # Sense box: freezer-2 temperature
    sense003 = "not set"  # Sense box: utility room humidity
    sense004 = "not set"  # Sense box: utility room air pressure
    sense005 = "not set"  # Sense box: alarm status

    print ("decoding: " + datastring)

    # data from an AQM IoT 'satellite' sensor system e.g. https://onlinedevices.org.uk/Air+Quality+Monitoring+IoT+project
    # ----------------------------------------------
    if datastring[0:8] == "AQsys001":    # hallway air temperature deg.C
        nowepoch = round(time.time())
        # the sent data includes the timestamp (epoch integer) that it was collected
        sentepoch = int( find_between( datastring, ":", "END" ) )
        sentdatetime = datetime.fromtimestamp(sentepoch)
        sentdatetime = sentdatetime.strftime('%a %d %b %Y %H:%M:%S %Z')
        # do a check to only use received data that is not older than a set amount of time in seconds
        if (nowepoch - sentepoch) < 300:   # data is less than 5 minutes old
            AQsys001 = datastring[11:17]
            # left as a text string but can be converted to a floating point variable e.g. AQsys001 = float(AQsys001)
        else:
            AQsys001 = "*too old!*"

        print ("temperature: " + AQsys001 + "deg.C received at " + sentdatetime )

    # checks for additional AQsysNNN data can be added here


    # data from a Sense Box IoT satellite sensor system e.g. https://onlinedevices.org.uk/RPi+Maker+PCB+-+Sensor+box+project
    # -------------------------------------------------
    elif datastring[0:8] == "sense001":      # freezer-1 temperature deg.C
        nowepoch = round(time.time())
        # the sent data includes the timestamp (epoch integer) that it was collected
        sentepoch = int( find_between( datastring, ":", "END" ) )
        sentdatetime = strftime('%a %d %b %Y %H:%M:%S %Z', localtime(sentepoch))
        # do a check to only use received data that is not older than a set amount of time in seconds
        if (nowepoch - sentepoch) < 300:  # data is less than 5 minutes old
            sense001 = datastring[11:17]

        else:
            sense001 = "*too old!*"

        print ("temperature: " + sense001 + "deg.C received at " + sentdatetime )

    elif datastring[0:8] == "sense002":      # freezer-2 temperature deg.C
        nowepoch = round(time.time())
        # the sent data includes the timestamp (epoch integer) that it was collected
        sentepoch = int( find_between( datastring, ":", "END" ) )
        sentdatetime = strftime('%a %d %b %Y %H:%M:%S %Z', localtime(sentepoch))
        # do a check to only use received data that is not older than a set amount of time in seconds
        if (nowepoch - sentepoch) < 300:  # data is less than 5 minutes old
            sense002 = datastring[11:17]

        else:
            sense002 = "*too old!*"

        print ("temperature: " + sense002 + "deg.C received at " + sentdatetime )

    # checks for additional senseNNN data can be added here



#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
# find a substring between two substrings within a main string
#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
def find_between( mainstring, firstsub, lastsub ):  # finds the first occurence
    try:
        start = mainstring.index( firstsub ) + len( firstsub )
        end = mainstring.index( lastsub, start )
        return mainstring[start:end]
    except ValueError:
        return ""

def find_between_r( mainstring, firstsub, lastsub ):  # finds the last occurence
    try:
        start = mainstring.rindex( firstsub ) + len( firstsub )
        end = mainstring.rindex( lastsub, start )
        return mainstring[start:end]
    except ValueError:
        return ""


##############################################
##   main
##############################################

global client1IP, client2IP
# possible sending client device IP addresses defined here
client1IP = "xxx.xxx.xxx.xxx"
client2IP = "xxx.xxx.xxx.xxx"

# various possible data msg labels for indicating data sources 
# - which could also be matched to a specific IP address when attempting 
#   to decode the data - but this is not yet done
global AQsys001, AQsys002, AQsys003, AQsys004, AQsys005  # received data from an AQM IoT satellite system
global sense001, sense002, sense003, sense004, sense005  # received data from a general Sense Box IoT satellitesystem

# ---------------------
# imports and threading
# ---------------------
import time
from time import strftime, localtime
import datetime
import socket
import sys

# set up threading
import logging
import threading
threads = list()  # keep a list of all the threads that are opened

# start the main socket thread
# start new thread: 1st argument as a function name to be run, 
#                   2nd is the tuple of arguments to the function
#                   3rd sets daemon True so that thread stops immediately when the main 'thread' closes
print("starting main socket thread .........")
x = threading.Thread(target=socketthread, args=(1,), daemon=True)  # just use a simple numeric for the thread identifier
threads.append(x)
x.start()

# now run a continuous loop where the data would normally be analysed 
# but this just allows the threads to keep running so that
# they can just display the 'globals' that are being updated 

print ("running a continuous loop waiting for inputs")
while True:
    # short pause so that this loop does not 'hog' the processor
    time.sleep(3) 

