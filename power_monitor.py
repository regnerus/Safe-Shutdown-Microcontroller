#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
# title: I2C Master Script
# desription: Periodically requests information from Safe Shutdown v2.0
# author: Camble
# date 2016-11-15
# version: 0.1
# source: https://github.com/Camble/Safe-Shutdown-Microcontroller

import smbus
import struct
import time
from subprocess import call

bus = smbus.SMBus(1)
system_state = 0
voltage = 0
warn_voltage = 3.3
shutdown_voltage = 3.2
address = 0x04

def configSlave(value):
    # write some settings to the slave via I2C
    # bus.write_byte(address, value)
    return -1

def getState(block):
    system_state = struct.unpack('<h', ''.join([chr(i) for i in block[0:2]]))[0]
    print "State", system_state

def compareVoltage(block):
    voltage = struct.unpack('<f', ''.join([chr(i) for i in block[2:6]]))[0]
    print "Voltage", voltage
    
    if voltage <= warn_voltage and voltage > shutdown_voltage:
        pass # warn if warn_voltage reached
    elif voltage <= shutdown_voltage:
        call("sudo nohup shutdown -h now", shell=True) # shutdown shutdown_voltage reached

while True:
    time.sleep(1)
    block = bus.read_i2c_block_data(address, 0)
    
    getState(block)
    compareVoltage(block)
