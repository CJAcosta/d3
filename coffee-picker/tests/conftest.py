import os

# The picker tests render camera images. Without a display, use software
# OpenGL (apt: libosmesa6). Must be set before mujoco is imported.
if not os.environ.get("DISPLAY"):
    os.environ.setdefault("MUJOCO_GL", "osmesa")
