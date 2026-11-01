Tab5OS 0.8.16 - native resource recovery, removable volumes and font fallback

From a shell with a working host C++ compiler:
  python run.py PATH_TO_SKETCH --cxx g++
  python run.py PATH_TO_SKETCH --cxx g++ --volume
  python test_locale.py PATH_TO_SKETCH --cxx g++

On Windows with Visual Studio 2022 Community at its default location:
  run_msvc.cmd PATH_TO_SKETCH

The resource and volume tests compile the actual firmware .cpp files without
changes to their logic. Arduino/FS adapters are mocked to inject short writes,
corrupt readback, rename failure, concurrent destination creation and a path
collision. They also verify no overwrite of existing or empty user files,
cancellation cleanup, reference counting, independent USB access, exclusive
maintenance ownership, and deliberate ejection followed by manual mounting.

test_locale.py executes the actual font-switch and translation lookup method
bodies against the generated tables. It checks every ASCII fallback entry,
sorting, printf placeholder compatibility, temporary override, restoration,
and preservation of the selected language. It does not emulate NVS or SD
language-pack parsing.

These tests do not replace real Tab5 trials: unplug/replug, filesystem errors,
SD latency, interrupted power, graphical rendering and native USB hardware
must still be tested on the device.
