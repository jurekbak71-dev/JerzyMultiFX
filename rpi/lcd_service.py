#!/usr/bin/env python3
import argparse,json,time
from pathlib import Path
from control import rpc
from lcd import gpio_driver

def lines(state):
    first='Jerzy MultiFX'
    second=f"OUT {state['out']:.2f} X{state['xruns']}"
    return first[:16].ljust(16),second[:16].ljust(16)

def main():
    p=argparse.ArgumentParser();p.add_argument('--config',type=Path,required=True);a=p.parse_args();c=json.loads(a.config.read_text())
    if c.get('electrical_review_complete') is not True:
        raise SystemExit('Verify physical module pinout, 3.3V/5V compatibility, RW LOW, power and backlight first.')
    driver=gpio_driver(c['chip'],c['pins']);driver.initialize()
    try:
        while True:
            try:driver.display(*lines(rpc('STATE')))
            except OSError:driver.display('Jerzy MultiFX','AUDIO OFFLINE')
            time.sleep(.25)
    finally:driver.request.release()
if __name__=='__main__':main()
