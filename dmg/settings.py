# dmgbuild settings for build-dmg.sh, which passes these values with -D
import json
import os

staging = defines["staging"]
layout = json.load(open(defines["layout"]))

files = [os.path.join(staging, name) for name in sorted(os.listdir(staging))]
hide = [defines["hidden"]]
format = "UDZO"

# background.png, with @2x beside it for Retina
background = defines["background"]
window_rect = ((200, 120), (layout["width"], layout["height"]))
icon_size = layout["iconSize"]
text_size = layout["textSize"]
icon_locations = {defines["app"]: (layout["iconX"], layout["iconY"])}
