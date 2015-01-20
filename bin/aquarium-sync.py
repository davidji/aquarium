#!/usr/bin/python

import os
import gobject
import time
import sys
import dbus
import dbus.service
import dbus.mainloop.glib

from time import localtime, sleep, strftime, time, clock
from select import select

from bluetooth import *

import dbus.exceptions

from optparse import OptionParser

PROGRAM = """
set inverted true\r
set program 0 sunrise 09:00 0\r
set program 1 daylight 11:00 180\r
set program 2 sunset 20:00 180\r
set program 3 moonlight 22:00 2\r
set program 4 darkness 11:45 0\r
set time %(time)s\r
auto\r
"""

class Agent(dbus.service.Object):

    def __init__(self, bus, path, pin):
        dbus.service.Object.__init__(self, bus, path)
        self.pin = pin

    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="", out_signature="")
    def Release(self):
        print "Release"

    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="os", out_signature="")
    def Authorize(self, device, uuid):
        raise Rejected("Connection rejected by user")

    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="o", out_signature="s")
    def RequestPinCode(self, device):
        return self.pin
 
    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="o", out_signature="u")
    def RequestPasskey(self, device):
        print "RequestPasskey"
        raise Rejected("Unsupported")

    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="ou", out_signature="")
    def DisplayPasskey(self, device, passkey):
        print "DisplayPasskey"
        raise Rejected("Unsupported")

    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="ou", out_signature="")
    def RequestConfirmation(self, device, passkey):
        print "RequestConfirmation"
        raise Rejected("Passkey doesn't match")

    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="s", out_signature="")
    def ConfirmModeChange(self, mode):
        print "ConfirmModeChange"
        raise Rejected("Mode change by user")

    @dbus.service.method(
        "org.bluez.Agent",
        in_signature="", out_signature="")
    def Cancel(self):
        print "Cancel"

class Search:
    def __init__(self, address, agent, adapter, mainloop):
        self.agent = agent
        self.address = address
        self.adapter = adapter
        self.mainloop = mainloop

    def register(self):
        self.found = True
        def device_found(address, properties):
            if not(self.found) and address == self.address:
                print "Found it! (%s)" % self.address
                self.found = True
                self.pair()
            else:
                print "Ignoring %s" % address

        self.adapter.connect_to_signal(
            handler_function=device_found,
            signal_name='DeviceFound')

    def remove(self):
        try:
            device = self.adapter.FindDevice(self.address)
            self.adapter.RemoveDevice(device)
            print "Removed %s" % device
        except dbus.exceptions.DBusException, e:
            print "Did not remove %s" % e


    def start(self):
        self.found = False
        self.adapter.StartDiscovery()

    def pair(self):
        def create_device_reply(device):
            print "New device (%s)" % (device)
            self.adapter.StopDiscovery()
            self.mainloop.quit()

        def create_device_error(error):
            print "Creating device failed: %s" % (error)

        self.adapter.CreatePairedDevice(
            self.address, 
            self.agent,
            "DisplayYesNo",
            reply_handler=create_device_reply,
            error_handler=create_device_error)

    def synchronize(self):
        socket=BluetoothSocket(RFCOMM)
        socket.connect((self.address, 1))

        def isotime():
            return strftime('%Y-%m-%dT%H:%M:%S', localtime())

        def readFor(timeout):
            seconds = time()
            expires = seconds + timeout
            while seconds < expires:
                (r, w, x) = select(
                    [ socket.fileno() ], [], [], 
                    expires - seconds)
                if len(r):
                    sys.stdout.write(socket.recv(1024))
                seconds = time()
            sys.stdout.flush()

        try:
            for line in (PROGRAM % { 'time': isotime() }).split('\n'):
                socket.sendall("%s\n" % line)
                readFor(0.1)
            while True:
                readFor(60.0)
                socket.sendall("set time %s\r\n" % isotime())
        finally:
            socket.close()

def main(args):
    parser = OptionParser()
    parser.add_option(
        "--adapter", action="store",
        type="string", dest="adapter",
        default=None)
    parser.add_option(
        "--agent-path", action="store",
        type="string", dest="agent_path",
        default='/aquarium/agent')
    parser.add_option(
        "--log-file", action="store",
        type="string", dest="log_file",
        default='/var/log/home-automation/aquarium.log')
    parser.add_option(
        "--job", action="store_true",
        dest="job",
        default=False,
        help="This is an upstart job, disconnect from console")
    (options, args) = parser.parse_args()

    if options.job:
        logfile = os.open(options.log_file, os.O_WRONLY|os.O_APPEND|os.O_CREAT)
        os.dup2(os.open('/dev/null', os.O_RDONLY), 0)
        os.dup2(logfile, 1)
        os.dup2(logfile, 2)

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    mainloop = gobject.MainLoop()
    bus = dbus.SystemBus()
    manager = dbus.Interface(
        bus.get_object("org.bluez", "/"),
        "org.bluez.Manager")

    if options.adapter:
        adapter_path = manager.FindAdapter(options.adapter)
    else:
        adapter_path = manager.DefaultAdapter()

    print adapter_path

    adapter = dbus.Interface(
        bus.get_object("org.bluez", adapter_path),
        "org.bluez.Adapter")

    agent = Agent(bus, options.agent_path, args[1])
    search = Search(args[0], options.agent_path, adapter, mainloop)

    search.remove()
    search.register()
    print "listening"
    search.start()
    print "Started search"
    mainloop.run()
    try:
        search.synchronize()
    except BluetoothError, e:
        print "exiting %s" % str(e)

if __name__ == '__main__':
    main(sys.argv)
