"""Zoom/rate policy mirrored in the tablet importer (tab5mappolicy)."""
MAX_VIEW_ZOOM = 22


def native_range(low, high, native_max):
    low, high, native_max = int(low), int(high), int(native_max)
    if not 0 <= low <= high <= MAX_VIEW_ZOOM or not 0 <= native_max <= MAX_VIEW_ZOOM:
        raise ValueError('Zooms : 0 à 22')
    # A high display zoom reuses the native parent, never requests absent tiles.
    return min(low, native_max), min(high, native_max)


def request_rate(value, public_topo=False, maximum=100.0):
    rate = float(value)
    if not 0.1 <= rate <= maximum:
        raise ValueError('Fréquence de requêtes invalide')
    return min(rate, 0.5) if public_topo else rate
