# Alarm

Open Alarm from the built-in applications on the home screen. First check the date, time and time zone in Settings. Enable the switch, choose a time and either Once or Every day, then select Save. Next alarm shows the saved alarm time. Changes to fields take effect only after Save.

The alarm volume is separate from the normal volume. Test alarm saves the settings and plays for ten seconds; Stop ends the test immediately. Test the sound with the intended audio output: normal speaker or headphone routing is preserved.

At the scheduled time, the screen turns back on and the alarm plays for up to five minutes. Stop or Escape ends it. Snooze for 5 minutes, or Space while the alarm is sounding, schedules another reminder. Audio playback may be paused and resumed by the shared driver; active video playback, an ELF application and recording receive a stop request to make the audio output available. Audio failures remain visible on screen.

Settings are stored in internal memory. An alarm less than ten minutes overdue may sound after a restart; an older alarm is marked as missed and will not sound days later. A daily alarm is recalculated in local time and sounds only once during the repeated hour when daylight saving time ends.

## Choosing a sleep mode

Normal screen sleep preserves running applications. The alarm also works in this mode, but the OS continues running.

Sleep until alarm saves more power. Save documents, finish applications/transfers and safely eject any USB storage. The OS refuses to shut down while an activity or write is still in progress. If Wi-Fi is active, the first press requests that it stop; press again after it has stopped. The alarm must be saved and at least three minutes away.

The RX8130CE RTC keeps the countdown and controls the hardware wake-up. The P4 and C6 do not stay active to count seconds. Restart is scheduled about two minutes before the alarm to allow for hardware rounding and startup time; the sound then waits for its scheduled time. Application sessions in RAM are not resumed. Pressing the button can also turn the tablet back on.

Keep the battery charged or connect a power supply. Battery charging stops while the Tab5 is off: use screen sleep to charge overnight. The hardware double-press power-off action is not equivalent to Sleep until alarm and does not schedule this wake-up.

Before relying on it as a daily alarm, test an alarm a few minutes ahead on the tablet, then test prolonged sleep on battery and/or external power. Actual power consumption and physical wake-up still need to be measured.

Hardware reference: [M5Stack Tab5](https://docs.m5stack.com/en/core/Tab5) (RX8130CE RTC, wake-up control and charging while powered on).
