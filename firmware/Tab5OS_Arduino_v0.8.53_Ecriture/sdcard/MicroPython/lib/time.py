from _tab5 import call

def ticks_ms(): return call(0)
def ticks_add(ticks, delta): return (ticks + delta) & 0x3fffffff
def ticks_diff(new, old): return ((new - old + 0x20000000) & 0x3fffffff) - 0x20000000
def sleep_ms(milliseconds): call(1, int(milliseconds))
def sleep(seconds): sleep_ms(int(seconds * 1000))
