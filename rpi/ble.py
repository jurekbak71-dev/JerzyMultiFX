#!/usr/bin/env python3
"""BlueZ GATT: paired encrypted control plus a separate BLE-MIDI service.
Radio/iOS interoperability must be validated on hardware; --self-test needs no radio.
"""
import json, sys
from control import ble_command, ble_midi, rpc
CONTROL_SERVICE='4a4d4658-7101-48a5-b81d-27aec3d64010'
CONTROL_CHAR='4a4d4658-7101-48a5-b81d-27aec3d64011'
MIDI_SERVICE='03b80e5a-ede8-4b33-a751-6ce34ec4c700'
MIDI_CHAR='7772e5db-3868-4112-a1a9-f2669d106bf3'

def main():
    import dbus, dbus.service
    from dbus.mainloop.glib import DBusGMainLoop
    from gi.repository import GLib
    DBusGMainLoop(set_as_default=True)
    bus=dbus.SystemBus();root='/dev/jerzy/multifx';props='org.freedesktop.DBus.Properties';gatt='org.bluez.GattCharacteristic1'
    class Object(dbus.service.Object):
        def __init__(self,path,interfaces):
            self.interfaces=interfaces;self.path=dbus.ObjectPath(path);super().__init__(bus,path)
        @dbus.service.method(props,in_signature='ss',out_signature='v')
        def Get(self,interface,key): return self.interfaces[interface][key]
        @dbus.service.method(props,in_signature='s',out_signature='a{sv}')
        def GetAll(self,interface): return self.interfaces.get(interface,{})
        @dbus.service.signal(props,signature='sa{sv}as')
        def PropertiesChanged(self,interface,changed,invalidated):pass
    objects=[]
    class Characteristic(Object):
        def __init__(self,path,service,uuid,midi=False):
            super().__init__(path,{gatt:{'UUID':uuid,'Service':dbus.ObjectPath(service),'Flags':dbus.Array(['read','write','notify','encrypt-authenticated-read','encrypt-authenticated-write'],signature='s')}})
            self.midi=midi;self.value=b'{"ok":true}';self.notify=False;self.page=0
        @dbus.service.method(gatt,in_signature='a{sv}',out_signature='ay')
        def ReadValue(self,options):
            offset=int(options.get('offset',0))
            return dbus.Array(self.value[offset:],signature='y')
        @dbus.service.method(gatt,in_signature='aya{sv}',out_signature='')
        def WriteValue(self,data,options):
            if int(options.get('offset',0))!=0 or len(data)>512:
                raise dbus.exceptions.DBusException('Unsupported length/offset',name='org.bluez.Error.InvalidValueLength')
            try:
                if self.midi:
                    for event in ble_midi(data):
                        rpc('MIDI '+' '.join(str(x) for x in event))
                    response={'ok':True}
                else:
                    message=json.loads(bytes(data));self.page=int(message.get('start',int(message['id'])//16*16 if message.get('op')=='set' else self.page));response=ble_command(message)
                self.value=json.dumps(response,separators=(',',':')).encode()
                if len(self.value)>512:raise ValueError('BLE payload exceeds 512 bytes')
                if self.notify:self.PropertiesChanged(gatt,{'Value':dbus.Array(self.value,signature='y')},[])
            except (ValueError,TypeError,KeyError,OSError) as e:
                raise dbus.exceptions.DBusException(str(e),name='org.bluez.Error.Failed')
        @dbus.service.method(gatt,in_signature='',out_signature='')
        def StartNotify(self):self.notify=True
        @dbus.service.method(gatt,in_signature='',out_signature='')
        def StopNotify(self):self.notify=False
        def poll(self):
            if self.notify and not self.midi:
                try:
                    self.value=json.dumps(ble_command({'op':'page','start':self.page}),separators=(',',':')).encode()
                    if len(self.value)<=512:self.PropertiesChanged(gatt,{'Value':dbus.Array(self.value,signature='y')},[])
                except (OSError,ValueError,KeyError):pass
            return True
    for index,(service_uuid,char_uuid) in enumerate(((CONTROL_SERVICE,CONTROL_CHAR),(MIDI_SERVICE,MIDI_CHAR))):
        path=f'{root}/service{index}'
        objects.append(Object(path,{'org.bluez.GattService1':{'UUID':service_uuid,'Primary':dbus.Boolean(True)}}))
        char=Characteristic(path+'/char0',path,char_uuid,index==1);objects.append(char);GLib.timeout_add(400,char.poll)
    class Application(Object):
        @dbus.service.method('org.freedesktop.DBus.ObjectManager',out_signature='a{oa{sa{sv}}}')
        def GetManagedObjects(self):return {o.path:o.interfaces for o in objects}
    app=Application(root,{})
    managed=dbus.Interface(bus.get_object('org.bluez','/'),'org.freedesktop.DBus.ObjectManager').GetManagedObjects()
    adapter=next((p for p,i in managed.items() if 'org.bluez.GattManager1' in i and 'org.bluez.LEAdvertisingManager1' in i),None)
    if adapter is None:raise SystemExit('No BlueZ GATT/LE advertising adapter. Hardware validation required.')
    manager=dbus.Interface(bus.get_object('org.bluez',adapter),'org.bluez.GattManager1')
    manager.RegisterApplication(app.path,{})
    class Advertisement(Object):
        @dbus.service.method('org.bluez.LEAdvertisement1',in_signature='',out_signature='')
        def Release(self):pass
    advert=Advertisement(root+'/advert',{'org.bluez.LEAdvertisement1':{'Type':'peripheral','ServiceUUIDs':dbus.Array([CONTROL_SERVICE,MIDI_SERVICE],signature='s'),'LocalName':'Jerzy MultiFX','Includes':dbus.Array(['tx-power'],signature='s')}})
    advertising=dbus.Interface(bus.get_object('org.bluez',adapter),'org.bluez.LEAdvertisingManager1');advertising.RegisterAdvertisement(advert.path,{})
    try:GLib.MainLoop().run()
    finally:
        advertising.UnregisterAdvertisement(advert.path);manager.UnregisterApplication(app.path)
if __name__=='__main__':main()
