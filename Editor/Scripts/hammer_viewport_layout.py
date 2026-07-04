# Switch Hammer's viewport layout and toggle maximize from Python.
# Run on demand from the Editor's Python console or "Run Script...".

import azlmbr.bus
import azlmbr.hammer


def set_viewport_count(count):
    azlmbr.hammer.HammerViewportRequestBus(azlmbr.bus.Broadcast, 'SetViewportCount', count)


def toggle_maximize():
    azlmbr.hammer.HammerViewportRequestBus(azlmbr.bus.Broadcast, 'ToggleMaximizeActiveViewport')
