import math
import tab5ui as ui
from time import sleep_ms

print(ui.tr('MicroPython sur Tab5OS'))
for angle in range(0, 91, 15):
    print(angle, math.sin(angle * math.pi / 180))
    sleep_ms(100)

# Capteur I2C externe : décommenter après connexion sur PORT.A.
# import tab5
# from machine import I2C
# tab5.external_power(True)
# bus = I2C(1, scl=54, sda=53)
# print(bus.scan())
# bus.deinit()
