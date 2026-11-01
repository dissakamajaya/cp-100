Import('env')
import runpy
from pathlib import Path
runpy.run_path(str(Path(env['PROJECT_DIR']) / 'tools' / 'embed_web.py'))
