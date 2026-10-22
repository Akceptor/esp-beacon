import serial, time, sys

PORT = '/dev/cu.usbserial-0001'
BAUD = 115200
SECS = 5

s = serial.Serial(PORT, BAUD, timeout=1, dsrdtr=False, rtscts=False)

# esptool-style reset pulse so we re-catch the boot banner
s.dtr = False; s.rts = False; time.sleep(0.1)
s.rts = True;  time.sleep(0.1)
s.dtr = True;  time.sleep(0.1)
s.dtr = False; time.sleep(0.1)
s.rts = False; time.sleep(0.2)
s.reset_input_buffer()

end = time.time() + SECS
out = b''
while time.time() < end:
    out += s.read(1024)
sys.stdout.write(out.decode('utf-8', 'replace'))
