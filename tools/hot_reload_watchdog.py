import threading
import time
from pathlib import Path

from watchdog.events import DirModifiedEvent, FileModifiedEvent, FileSystemEventHandler
from watchdog.observers import Observer

import adb


class WatchdogEventHandler(FileSystemEventHandler):

    def __init__(self, *, dst_dir: str | Path, debounce_seconds: float = 0.5):
        self._dst_dir = Path(dst_dir)
        self._debounce_seconds = debounce_seconds
        self._modified_files = set()
        self._timer = None

    def on_timer_expired(self):
        for path in self._modified_files:
            adb.push(path, self._dst_dir.as_posix())
        self._modified_files.clear()

    def on_modified(self, event: DirModifiedEvent | FileModifiedEvent):

        if event.is_directory:
            return

        path = Path(event.src_path)

        if path.name.endswith('~'):
            return

        self._modified_files.add(path)

        if self._timer:
            self._timer.cancel()

        self._timer = threading.Timer(self._debounce_seconds, self.on_timer_expired)
        self._timer.start()


def main():
    project_root_dir = Path(__file__).resolve().parent.parent
    assets_dir = project_root_dir / 'assets'

    observer = Observer()
    observer.schedule(WatchdogEventHandler(dst_dir=Path('/sdcard/')), assets_dir)
    observer.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        observer.stop()
    observer.join()


if __name__ == '__main__':
    main()
