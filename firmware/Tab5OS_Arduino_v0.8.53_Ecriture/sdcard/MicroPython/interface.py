"""Exemple tactile sans périphérique à brancher : compteur et curseur."""
import tab5ui as ui
from time import sleep_ms

ui.open(ui.tr('Mon interface Python'))
ui.label(ui.tr('Compteur tactile'))
counter = ui.label('0', y=75)
minus = ui.button('-1', x=20, y=145)
plus = ui.button('+1', x=280, y=145)
reset = ui.button(ui.tr('Remise à zéro'), x=540, y=145)
level = ui.slider(y=270, maximum=100, value=50)
level_text = ui.label('50 %', y=330)
quit_button = ui.button(ui.tr('Fermer'), y=400)
value = 0

try:
    while ui.is_open():
        event = ui.poll()
        if event:
            widget_id, position = event
            if widget_id == minus.id:
                value -= 1
            elif widget_id == plus.id:
                value += 1
            elif widget_id == reset.id:
                value = 0
            elif widget_id == level.id:
                level_text.text(str(position) + ' %')
            elif widget_id == quit_button.id:
                ui.close()
            counter.text(value)
        sleep_ms(20)
finally:
    ui.close()
