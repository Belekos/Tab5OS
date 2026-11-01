"""Tab5OS peripheral API. GPIO numbers are ESP32-P4 numbers."""
from _tab5 import call

class Pin:
    IN, OUT, OPEN_DRAIN = 0, 1, 2
    PULL_UP, PULL_DOWN = 1, 2
    def __init__(self, id, mode=IN, pull=None, *, value=None):
        self.id = int(id)
        call(2, self.id, mode, pull or 0)
        if value is not None:
            self.value(value)
    def value(self, value=None):
        if value is None:
            return call(3, self.id)
        call(4, self.id, int(bool(value)))
    __call__ = value
    def on(self): self.value(1)
    def off(self): self.value(0)
    def deinit(self): call(9, self.id)

def _pin(pin):
    return pin.id if isinstance(pin, Pin) else int(pin)

class I2C:
    def __init__(self, id=1, *, scl=54, sda=53, freq=100000):
        if id != 1: raise ValueError('Only external I2C 1 is available')
        call(10, _pin(sda), _pin(scl), freq)
    def writeto(self, address, buffer, stop=True):
        return call(11, address, int(stop), 0, buffer)
    def readfrom(self, address, nbytes, stop=True):
        if nbytes == 0: return b''
        return call(12, address, int(stop), 0, None, nbytes)
    def scan(self):
        found = []
        for address in range(8, 120):
            try: self.writeto(address, b''); found.append(address)
            except OSError: pass
        return found
    def readfrom_mem(self, address, register, nbytes, *, addrsize=8):
        if addrsize not in (8, 16): raise ValueError('addrsize')
        self.writeto(address, register.to_bytes(addrsize // 8, 'big'), False)
        return self.readfrom(address, nbytes)
    def writeto_mem(self, address, register, buffer, *, addrsize=8):
        if addrsize not in (8, 16): raise ValueError('addrsize')
        return self.writeto(address, register.to_bytes(addrsize // 8, 'big') + bytes(buffer))
    def deinit(self): call(13)

class SPI:
    MSB = 0
    def __init__(self, id=2, *, baudrate=1000000, polarity=0, phase=0, bits=8,
                 firstbit=MSB, sck=5, mosi=18, miso=19):
        if id != 2 or bits != 8 or firstbit != self.MSB:
            raise ValueError('SPI 2: 8-bit MSB only')
        self.baudrate, self.mode = baudrate, polarity * 2 + phase
        call(20, _pin(sck), _pin(mosi), _pin(miso))
    def write_readinto(self, source, destination):
        if len(source) != len(destination): raise ValueError('buffer sizes differ')
        if not source: return
        destination[:] = call(21, self.baudrate, self.mode, 0, source, len(source))
    def write(self, source):
        call(21, self.baudrate, self.mode, 0, source, len(source))
    def read(self, nbytes, write=0):
        if nbytes == 0: return b''
        return call(21, self.baudrate, self.mode, 0, bytes([write]) * nbytes, nbytes)
    def deinit(self): call(22)

class UART:
    def __init__(self, id=1, baudrate=115200, *, tx=6, rx=7):
        if id != 1: raise ValueError('Only UART 1 is available')
        call(30, _pin(tx), _pin(rx), baudrate)
    def write(self, buffer):
        if isinstance(buffer, str): buffer = buffer.encode()
        return call(31, 0, 0, 0, buffer)
    def read(self, nbytes=128):
        if nbytes == 0: return b''
        data = call(32, 0, 0, 0, None, nbytes)
        return data if data else None
    def deinit(self): call(33)
    def any(self): return call(34)
    def txdone(self): return bool(call(35))

class PWM:
    def __init__(self, pin, *, freq=1000, duty_u16=0):
        self.pin, self._freq, self._duty = _pin(pin), freq, duty_u16
        call(6, self.pin, freq, duty_u16)
    def freq(self, value=None):
        if value is None: return self._freq
        self._freq = value
        call(6, self.pin, self._freq, self._duty)
    def duty_u16(self, value=None):
        if value is None: return self._duty
        self._duty = value
        call(6, self.pin, self._freq, self._duty)
    def deinit(self): call(7, self.pin)

class ADC:
    ATTN_0DB, ATTN_2_5DB, ATTN_6DB, ATTN_11DB = 0, 1, 2, 3
    def __init__(self, pin, *, atten=ATTN_11DB):
        self.pin, self._atten = _pin(pin), atten
    def atten(self, value): self._atten = value
    def read(self): return call(5, self.pin, self._atten, 0)
    def read_u16(self): return self.read() * 65535 // 4095
    def read_uv(self): return call(5, self.pin, self._atten, 1) * 1000
    def deinit(self): call(9, self.pin)

def pin_available(pin):
    """0: reserved by board; 1: available to this app; 2: another app owns it."""
    return call(36, _pin(pin))
