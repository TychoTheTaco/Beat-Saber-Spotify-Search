import argparse
import subprocess
from os import environ
from pathlib import Path
from typing import List, Optional

from ansi import Color


def main():
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', '-i', required=True)
    parser.add_argument('--symbols', '-s')
    args = parser.parse_args()

    input_arg: str = args.input
    symbols_arg: Optional[str] = args.symbols

    temp_dir = Path('./temp')
    temp_dir.mkdir(exist_ok=True)

    symbols_dir = temp_dir
    if symbols_arg:
        symbols_dir = Path(symbols_arg)

    # Pull the debug library from the device
    if not symbols_arg:
        print(Color.CYAN('Pulling libraries from device...'))
        process = subprocess.run([
            'adb',
            'pull',
            '/sdcard/ModData/com.beatgames.beatsaber/Modloader/mods/libspotify-search.so',
        ], cwd=temp_dir, stderr=subprocess.STDOUT)

    total_paths: List[Path] = []

    match input_arg:
        case 'logs':
            # Pull the logs from the device
            log_file_dir = Path('/sdcard/ModData/com.beatgames.beatsaber/logs2')
            log_file_paths = [
                log_file_dir / 'spotify-search.log',
                log_file_dir / 'spotify-search.1.log'
            ]
            for path in log_file_paths:
                process = subprocess.run([
                    'adb',
                    'pull',
                    path.as_posix(),
                ], cwd=temp_dir, stderr=subprocess.STDOUT)
                total_paths.append(temp_dir / path.name)
        case 'logcat':
            # Get logcat output
            logcat_output_path = temp_dir / 'logcat.txt'
            with open(logcat_output_path, 'w') as file:
                process = subprocess.run([
                    'adb',
                    'shell',
                    'logcat -d *:F'
                ], cwd=temp_dir, stdout=file)
            total_paths.append(logcat_output_path)
        case _:
            total_paths.append(Path(input_arg))

    ndk_path = Path(environ.get('ANDROID_NDK_HOME'))
    ndk_stack_exe = ndk_path / 'ndk-stack.cmd'

    project_root_dir = (Path(__file__) / '..' / '..').resolve()
    build_output_dir = project_root_dir / 'build'

    print(ndk_stack_exe)

    for path in total_paths:
        print(Color.CYAN(f'Decoding: {path}'))
        process = subprocess.run([
            str(ndk_stack_exe.absolute()),
            '-sym',
            str(symbols_dir),
            '-i',
            path.absolute()
        ], stderr=subprocess.STDOUT)


if __name__ == '__main__':
    main()
