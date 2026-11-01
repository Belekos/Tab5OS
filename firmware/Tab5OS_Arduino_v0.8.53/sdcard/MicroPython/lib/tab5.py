from _tab5 import call

def external_power(enabled=True):
    """Switch the external expansion 5 V supply; does not change USB power or charging."""
    call(40, int(bool(enabled)))


def _env_name(name):
    value = name.encode("utf-8")
    if b"\0" in value:
        raise ValueError("environment name")
    return value + b"\0"

def env(name, default=None):
    """Read public RAM value or protected reference; locked secrets raise OSError."""
    try:
        return call(107, 0, 0, 0, _env_name(name), 513).decode("utf-8")
    except OSError as error:
        if error.args[0] == 2:  # ENOENT only; never hide a locked-vault error.
            return default
        raise

def setenv(name, value):
    """Set a PUBLIC value for this boot, shared with sh/native/ELF/Python."""
    data = value.encode("utf-8")
    if b"\0" in data:
        raise ValueError("environment value")
    call(108, 0, 0, 0, _env_name(name) + data + b"\0")

def bindenv(name, secret_name=None):
    """Bind a protected vault entry by name; never copies its value."""
    call(108, 1, 0, 0, _env_name(name) + _env_name(secret_name or name))

def unsetenv(name):
    """Remove the runtime binding, leaving any encrypted vault entry intact."""
    call(109, 0, 0, 0, _env_name(name))

def env_names():
    """Names only. This deliberately does not expose any value."""
    return call(109, 1, 0, 0, None, 4096).decode("utf-8").splitlines()
