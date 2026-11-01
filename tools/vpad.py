#!/usr/bin/env python3
# tools/vpad.py NAME [RUMBLE_LOG]: a virtual Xbox 360 style controller through
# /dev/uinput, gone when this exits. REST=ly=25000,rt=255 in the environment
# leaves it resting off-centre from the start. Reads stdin: "a", "b", "start",
# "up", "down", "left", "right" press and release; "rt=255", "lx=-32768" and so
# on set an axis and leave it; "quit" ends. Rumble it is sent goes to RUMBLE_LOG.
import fcntl, os, select, struct, sys, threading, time
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT, UI_SET_FFBIT = 0x40045564, 0x40045565, 0x40045567, 0x4004556b
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502
UI_BEGIN_UP, UI_END_UP = 0xC06855C8, 0x406855C9
UI_BEGIN_ER, UI_END_ER = 0xC00C55CA, 0x400C55CB
EV_SYN, EV_KEY, EV_ABS, EV_FF, EV_UINPUT, FF_RUMBLE = 0, 1, 3, 0x15, 0x0101, 0x50
BTN = {'a': 0x130, 'b': 0x131, 'x': 0x133, 'y': 0x134, 'lb': 0x136, 'rb': 0x137,
       'back': 0x13a, 'start': 0x13b, 'guide': 0x13c, 'l3': 0x13d, 'r3': 0x13e}
AXIS = {'lx': 0, 'ly': 1, 'lt': 2, 'rx': 3, 'ry': 4, 'rt': 5}
HX, HY = 0x10, 0x11
HAT = {'up': (HY, -1), 'down': (HY, 1), 'left': (HX, -1), 'right': (HX, 1)}
name = sys.argv[1]; rlog = sys.argv[2] if len(sys.argv) > 2 else None
fd = os.open('/dev/uinput', os.O_RDWR | os.O_NONBLOCK)
for e in (EV_KEY, EV_ABS, EV_FF):
    fcntl.ioctl(fd, UI_SET_EVBIT, e)
fcntl.ioctl(fd, UI_SET_FFBIT, FF_RUMBLE)
for b in BTN.values():
    fcntl.ioctl(fd, UI_SET_KEYBIT, b)
absmax = [0] * 64; absmin = [0] * 64
for a in ('lx', 'ly', 'rx', 'ry'):
    fcntl.ioctl(fd, UI_SET_ABSBIT, AXIS[a]); absmax[AXIS[a]] = 32767; absmin[AXIS[a]] = -32768
for a in ('lt', 'rt'):
    fcntl.ioctl(fd, UI_SET_ABSBIT, AXIS[a]); absmax[AXIS[a]] = 255
for a in (HX, HY):
    fcntl.ioctl(fd, UI_SET_ABSBIT, a); absmax[a] = 1; absmin[a] = -1
dev = name.encode()[:79].ljust(80, b'\0') + struct.pack('<HHHH', 3, 0x045e, 0x028e, 0x110) + struct.pack('<I', 16)
dev += struct.pack('<64i', *absmax) + struct.pack('<64i', *absmin) + struct.pack('<64i', *[0] * 64) + struct.pack('<64i', *[0] * 64)
os.write(fd, dev); fcntl.ioctl(fd, UI_DEV_CREATE)
lock = threading.Lock()
def ev(t, c, v):
    with lock:
        os.write(fd, struct.pack('<qqHHi', 0, 0, t, c, v))
def syn():
    ev(EV_SYN, 0, 0)
stop = False; plays = 0; strong = 0; weak = 0
def rumble():
    global plays, strong, weak
    while not stop:
        if not select.select([fd], [], [], 0.2)[0]:
            continue
        try:
            d = os.read(fd, 24)
        except OSError:
            continue
        if len(d) < 24:
            continue
        t, c, v = struct.unpack('<HHi', d[16:24])
        if t == EV_UINPUT and c == 1:
            b = bytearray(struct.pack('<Ii', v, 0) + bytes(96))
            fcntl.ioctl(fd, UI_BEGIN_UP, b, True)
            s, w = struct.unpack_from('<HH', b, 8 + 16)
            strong = max(strong, s); weak = max(weak, w)
            b[4:8] = struct.pack('<i', 0); fcntl.ioctl(fd, UI_END_UP, b, True)
        elif t == EV_UINPUT and c == 2:
            b = bytearray(struct.pack('<IiI', v, 0, 0))
            fcntl.ioctl(fd, UI_BEGIN_ER, b, True)
            b[4:8] = struct.pack('<i', 0); fcntl.ioctl(fd, UI_END_ER, b, True)
        elif t == EV_FF and v:
            plays += 1
threading.Thread(target=rumble, daemon=True).start()
def axis(s):
    k, v = s.split('=')
    ev(EV_ABS, AXIS[k], int(v)); syn()
for r in filter(None, os.environ.get('REST', '').split(',')):
    axis(r)
print('up', flush=True)
for line in sys.stdin:
    k = line.strip()
    if k == 'quit':
        break
    if k in BTN:
        ev(EV_KEY, BTN[k], 1); syn(); time.sleep(0.12); ev(EV_KEY, BTN[k], 0); syn()
    elif k in HAT:
        a, v = HAT[k]
        ev(EV_ABS, a, v); syn(); time.sleep(0.12); ev(EV_ABS, a, 0); syn()
    elif '=' in k:
        axis(k)
stop = True; time.sleep(0.3)
if rlog:
    with open(rlog, 'a') as f:
        f.write(f'{name}: {plays} rumble plays, strongest {strong}/{weak}\n')
fcntl.ioctl(fd, UI_DEV_DESTROY); os.close(fd)
