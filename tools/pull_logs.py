import subprocess
from pathlib import Path


def adb_pull(path: Path):
    return subprocess.call(
        ['adb', 'pull', path.as_posix()]
    )


def main():
    log_file_dir = Path('/sdcard/ModData/com.beatgames.beatsaber/logs2')
    log_file_paths = [
        log_file_dir / 'spotify-search.log',
        log_file_dir / 'spotify-search.1.log'
    ]
    for path in log_file_paths:
        adb_pull(path)

    with open('logcat.txt', 'w') as file:

        process = subprocess.Popen([
            'adb',
            'shell',
            'logcat',
            '-d'
        ], stdout=file, stderr=file)

        process.wait()




if __name__ == '__main__':
    main()
