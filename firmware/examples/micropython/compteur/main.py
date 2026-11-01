"""Small Tab5OS app: shared theme, OS translations, shortcuts and cleanup."""
import tab5ui as ui
from time import sleep_ms


def main():
    value = 0
    ui.open(ui.tr("Compteur tactile"))
    try:
        reading = ui.label("0", x=20, y=30, width=ui.WIDTH - 40)
        less = ui.button("−", x=20, y=120, width=200)
        more = ui.button("+", x=240, y=120, width=200)
        reset = ui.button(ui.tr("Remise à zéro"), x=460, y=120, width=300)
        close = ui.button(ui.tr("Fermer"), x=ui.WIDTH - 240, y=ui.HEIGHT - 84, width=220)
        ui.shortcut(less, "previous")
        ui.shortcut(more, "next")
        ui.shortcut(close, "back")
        while ui.is_open():
            event = ui.poll()
            if event:
                widget, position = event
                if widget == close.id:
                    break
                if widget == less.id:
                    value -= 1
                elif widget == more.id:
                    value += 1
                elif widget == reset.id:
                    value = 0
                reading.text(value)
            sleep_ms(20)
    finally:
        ui.close()


main()
