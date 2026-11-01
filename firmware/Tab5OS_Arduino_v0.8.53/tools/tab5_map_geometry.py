"""Spherical circle/tile intersection shared by the PC map downloader."""
import math
EARTH_KM = 6371.0088
MAX_LAT = 85.0511287798066
RAD = math.pi / 180

def _tile_y(lat, n):
    phi = max(-MAX_LAT, min(MAX_LAT, lat)) * RAD
    return max(0, min(n-1, int((1-math.log(math.tan(math.pi/4+phi/2))/math.pi)*n/2)))


def _row_lat(y, n):
    e = math.exp(math.pi*(1-2*y/n))
    return math.atan((e-1/e)/2)


def rows(region, zoom, first_y=0):
    """Yield y, disjoint inclusive x spans, covering every circle/tile intersection."""
    n = 1 << zoom
    phi = region['lat']*RAD; rho = region['radius']/EARTH_KM
    sp = math.sin(phi); cp = math.cos(phi); cr = math.cos(rho)
    north = min(MAX_LAT*RAD, phi+rho); south = max(-MAX_LAT*RAD, phi-rho)
    top = max(first_y, _tile_y(north/RAD, n)); bottom = _tile_y(south/RAD, n)
    critical = math.asin(max(-1, min(1, sp/cr))) if abs(sp) <= cr else None
    for y in range(top, bottom+1):
        lo = max(south, _row_lat(y+1, n)); hi = min(north, _row_lat(y, n))
        if lo > hi + 1e-12:
            continue
        # At a fixed latitude the boundary satisfies cos(distance)=cos(radius).
        # The extrema occur at the row edges or sin(latitude)=sin(center)/cos(radius).
        ratio = min((cr-sp*math.sin(lo))/(cp*math.cos(lo)), (cr-sp*math.sin(hi))/(cp*math.cos(hi)))
        if critical is not None and lo <= critical <= hi:
            ratio = min(ratio, (cr-sp*math.sin(critical))/(cp*math.cos(critical)))
        if ratio <= -1:
            spans = ((0,n-1),)
        else:
            reach = math.acos(max(-1, min(1, ratio))) / RAD
            left = int(math.floor((region['lon']-reach+180)*n/360))
            right = int(math.floor((region['lon']+reach+180)*n/360))
            if right-left+1 >= n:
                spans = ((0,n-1),)
            else:
                left %= n; right %= n
                spans = ((left,right),) if left <= right else ((0,right),(left,n-1))
        yield y, spans


def estimate(region, progress=None):
    totals = []; count = 0; steps = 0
    for z in range(region['min_zoom'], region['max_zoom']+1):
        level = 0
        for y, spans in rows(region, z):
            for lo, hi in spans:
                level += hi-lo+1
            steps += 1
            if steps % 128 == 0:
                if progress:
                    progress(z, count+level)
                pass
        count += level; totals.append([z,level])
        if progress:
            progress(z, count)
    return count, totals


def tiles(region, cursor=None):
    cursor = cursor or [region['min_zoom'],0,0]
    for z in range(max(region['min_zoom'],cursor[0]), region['max_zoom']+1):
        for y, spans in rows(region, z, cursor[1] if z == cursor[0] else 0):
            for lo, hi in spans:
                if z == cursor[0] and y == cursor[1]:
                    lo = max(lo, cursor[2])
                for x in range(lo,hi+1):
                    yield z,x,y


