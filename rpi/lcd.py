"""HD44780 4-bit protocol. Physical pins must be configured after electrical review.
RW must be tied LOW; this write-only driver never reads a 5 V data bus.
"""
import time
class HD44780:
    def __init__(self,write,sleep=time.sleep):self.write=write;self.sleep=sleep
    def nibble(self,value,rs):
        self.write(rs,0,[(value>>i)&1 for i in range(4)])
        self.sleep(.000001)
        self.write(rs,1,[(value>>i)&1 for i in range(4)])
        self.sleep(.000001)
        self.write(rs,0,[(value>>i)&1 for i in range(4)])
        self.sleep(.00005)
    def byte(self,value,rs=0):self.nibble(value>>4,rs);self.nibble(value&15,rs)
    def initialize(self):
        self.sleep(.05)
        for delay in (.005,.001,.001):self.nibble(3,0);self.sleep(delay)
        self.nibble(2,0)
        for command in (0x28,0x08,0x01,0x06,0x0c):self.byte(command);self.sleep(.002)
    def display(self,first,second):
        for address,line in ((0x80,first),(0xc0,second)):
            self.byte(address)
            for c in line[:16].ljust(16):self.byte(ord(c) if 32<=ord(c)<127 else ord('?'),1)

def gpio_driver(chip,pins):
    """Explicit BCM line offsets: [RS,E,D4,D5,D6,D7], libgpiod v2.
    Does not select pins or power wiring automatically.
    """
    import gpiod
    from gpiod.line import Direction,Value
    if len(pins)!=6 or len(set(pins))!=6:raise ValueError('Six distinct verified GPIO offsets required')
    request=gpiod.request_lines(chip,consumer='JerzyMultiFX LCD',config={tuple(pins):gpiod.LineSettings(direction=Direction.OUTPUT,output_value=Value.INACTIVE)})
    def write(rs,e,data):request.set_values({p:Value.ACTIVE if v else Value.INACTIVE for p,v in zip(pins,[rs,e,*data])})
    driver=HD44780(write);driver.request=request;return driver
