import socket
import struct
import time
import serial
import math
import io
fanoff=0
fanmin=90
fanmax=180

minspeed=20.0
maxspeed=200.0

fanspeed=0

lastreading=0
lastreading1=0

HOST = '192.168.20.200'  # Standard loopback interface address (localhost)
PORT = 11200        # Port to listen on (non-privileged ports are > 1023)


def getArduino():
    ard=0
    while (ard==0):
        try:
            ard=serial.Serial('COM10', 9600, timeout=0.5)
        except serial.serialutil.SerialException:
            print("Arduino in use, retrying...")
            time.sleep(3)
    return ard

def getDialsServer():
    s=0
    while (s==0):
        try:
             s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
             s.connect((HOST, PORT))
             time.sleep(1)
        except ConnectionRefusedError:
            print("Dials Server not running, retying...")
            s=0
    return s

ard=getArduino()
time.sleep(3) #wait for bootloader

s=getDialsServer()
while 1:
    retry=0
    
    try:
        s.sendall(b'\x01')
        #print(s.recv(20))
        d1 = s.recv(4)
        d2 = s.recv(4)
        d3 = s.recv(4)
        d4 = s.recv(72)
        #print( s.recv(10))
    except ConnectionAbortedError:
        s=getDialsServer()
        retry=1
    if (not retry):
        airspeed=struct.unpack('f',d3)[0]
        if ((airspeed==lastreading) & (airspeed==lastreading1)): #cater for menu
            airspeed=0
        else:
            lastreading1=lastreading
            lastreading=airspeed
            if (airspeed<minspeed):
                airspeed=0
        airspeed=int(airspeed)
        #print('Airspeed %.2f' %(airspeed) )
        #print('Fanspeed %d' %(fanspeed) )
        outstring=b"%dX" % airspeed
        #print(outstring)
        ard.write(outstring) 
        b=ard.in_waiting
        while (b >0 ):
            print(ard.readline().decode())
            b=ard.in_waiting



        time.sleep(.2)
