#!/usr/bin/env python3

# Copyright 2015, Matthijs Kooijman <matthijs@stdin.nl>
#
# Permission is hereby granted, free of charge, to anyone
# obtaining a copy of this document and accompanying files, to do
# whatever they want with them without any restriction, including, but
# not limited to, copying, modification and redistribution.
#
# NO WARRANTY OF ANY KIND IS PROVIDED.
#
#
# This script is intended to work along with the Coordinator_Serial.ino
# sketch. That sketch collects measurements and sends them through the
# USB serial port. This script reads the measurements, stores them in an
# sqlite database and shows them in a graph using the matplotlib
# library.
#
# The format used is a single line containing:
#
# DATA:name_of_resource:value
#
# Any lines not satisfying this format will be printed, but not
# otherwise processed (still allowing debug output).
#
# This script requires python 2.7 or higher (3.x recommended), and the
# matplotlib library installed.
#
# You will likely need to modify the SERIAL variable below, to point to
# the serial port the Coordinator Arduino can be found on. On Windows,
# this uses a string like "COM8" to identify the serial port.
#
# Values will be stored in a sqlite database file, whose filename can be
# changed through the DBFILE variable below. By default, this creates a
# file in the directory from where this script was run, but you can
# specify an absolute pathname as well there.

import time
import serial
import sqlite3
import threading
from datetime import datetime
from matplotlib import pyplot
from matplotlib.dates import DateFormatter, AutoDateFormatter, AutoDateLocator

DBFILE = 'data.db'
BAUD = 115200

# For Linux, e.g.
SERIAL = '/dev/ttyACM0'
# For Windows, e.g.:
#SERIAL = 'COM12'
# For OSX, e.g.:
#SERIAL = '/dev/tty.usbmodem123'

lines = {}

def update_plot(conn, resource):
        """ Update the plot by (re)drawing the line for the given resource """

        # Get the last 25 samples of this resource to plot
        c = conn.cursor()
        c.execute("SELECT timestamp, value FROM data WHERE resource = ? ORDER BY timestamp DESC LIMIT 25;", (resource,))

        # Collect x and y data for this resource
        timestamps = []
        values = []
        for (timestamp, value) in c.fetchall():
                timestamps.append(datetime.utcfromtimestamp(timestamp))
                values.append(value)

        if resource not in lines:
                # New line - add it and update the legend
                lines[resource] = pyplot.plot(timestamps, values, 'o-', label=resource)[0]
                pyplot.legend(loc='upper left')
        else:
                # Existing line - update its data
                line = lines[resource]
                line.set_xdata(timestamps)
                line.set_ydata(values)

        # Update the limits and scales and redraw the plot
        pyplot.gca().relim()
        pyplot.gca().autoscale_view()
        pyplot.draw()

def record_data(conn, resource, value):
        """ Record the given datapoint at the current time """
        c = conn.cursor()
        c.execute("INSERT INTO data(timestamp, resource, value) VALUES (?, ?, ?)",
                  (time.time(), resource, value))
        conn.commit()

def read_serial(port):
        """ Read from the serial port and process any data received """
        # Open db inside the thread, since a connection object cannot
        # cross threads
        conn = sqlite3.connect(DBFILE)

        while True:
                # Read lines and process any data on lines starting with
                # "DATA:". All lines are printed, so any debug output
                # will also be visible.
                line = port.readline().decode().strip()
                print(line)
                parts = line.split(":")
                if len(parts) == 3 and parts[0] == "DATA":
                        record_data(conn, parts[1], parts[2])
                        update_plot(conn, parts[1])

def main():
        # Open the serial port
        try:
                port = serial.Serial(SERIAL, BAUD)
        except serial.SerialException as e:
                print("Failed to open serial port: " + str(e))
                return
        print("Serial port opened, waiting for data...");

        # Open the database
        conn = sqlite3.connect(DBFILE)

        # Create the table if it does not exist yet
        c = conn.cursor()
        c.execute('''CREATE TABLE IF NOT EXISTS data
                     (resource TEXT, timestamp INTEGER, value REAL)''')

        # Draw a line for all resources in the database
        c = conn.cursor()
        c.execute("SELECT DISTINCT resource FROM data")
        for (resource, ) in c.fetchall():
                update_plot(conn, resource)

        # Close database, so the other thread can open it
        conn.close()

        # Let matplotlib display times in the system timezone
        try:
                import tzlocal
                timezone = tzlocal.get_localzone()
                pyplot.rcParams['timezone'] = timezone
        except ImportError:
                print("tzlocal module not found, showing times in UTC")


        # Prevent confusing +2e1 offset notation when temperatures are
        # very close together
        pyplot.gca().yaxis.get_major_formatter().set_useOffset(False)

        # Run the serial processing in a separate thread. This allows
        # calling pyplot.show() below, which does not return until the
        # window is closed. By making this a daemon thread, it will be
        # automatically killed when the main thread completes (so when
        # the window is closed).
        t = threading.Thread(target = read_serial, args = (port,))
        t.daemon = True
        t.start()

        # Show the plot
        pyplot.show()

main()
