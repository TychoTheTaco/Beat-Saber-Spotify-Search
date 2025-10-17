import argparse
import json
import logging
import shutil
import signal
import subprocess
import time
from pathlib import Path
from subprocess import CompletedProcess
from typing import Any, Dict, List, Literal, Optional, Set, get_args
from zipfile import ZIP_DEFLATED, ZipFile

logging.basicConfig(
    format='%(asctime)s [%(name)s] [%(levelname)s] %(message)s',
    level=logging.INFO,
    datefmt='%m/%d/%Y %H:%M:%S'
)

def get_logger(name: str):
    return logging.getLogger(name)

logger = get_logger('Build Script')

PACKAGE_NAME_BEAT_SABER = 'com.beatgames.beatsaber'

is_app_running = False

# Capture the original signal handler
original_on_sig_int = signal.getsignal(signal.SIGINT)

def on_sig_int(sig: int, frame):
    if is_app_running:
        stop_app(PACKAGE_NAME_BEAT_SABER)
    else:
        if callable(original_on_sig_int):
            original_on_sig_int(sig, frame)

signal.signal(signal.SIGINT, on_sig_int)

def adb_shell(command: List[str]) -> CompletedProcess:
    process = subprocess.run([
        'adb',
        'shell',
        *command
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return process

def stop_app(app_id: str):
    process = subprocess.run([
        'adb',
        'shell',
        'am',
        'force-stop',
        app_id
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if process.returncode != 0:
        logger.error(f'Failed to stop app: {app_id}')
        logger.error(process.stdout.decode('utf-8'))

def start_app(app_id: str):
    process = adb_shell([
        'am',
        'start',
        'com.beatgames.beatsaber/com.unity3d.player.UnityPlayerActivity'
    ])
    if process.returncode != 0:
        logger.error(f'Failed to start app: {app_id}')
        logger.error(process.stdout.decode('utf-8'))


BuildType = Literal['debug', 'release']


def build(*, project_dir: str | Path, output_dir: str | Path, build_type: BuildType = 'debug'):
    project_dir = Path(project_dir).resolve().absolute()
    output_dir = Path(output_dir).resolve().absolute()

    # Determine the CMake build type
    cmake_build_type = None
    match build_type:
        case 'debug':
            cmake_build_type = 'RelWithDebInfo'
        case 'release':
            cmake_build_type = 'Release'
        case _:
            raise ValueError(f'Unknown build type: {build_type}')

    # Configure CMake
    logger.info(f'Configuring CMake project...')
    process = subprocess.run([
        'cmake',
        '-G',
        'Ninja',
        f'-DCMAKE_BUILD_TYPE={cmake_build_type}',
        '-B',
        str(output_dir)
    ], cwd=project_dir)
    if process.returncode != 0:
        raise RuntimeError(f'Command Failed! Exit Code = {process.returncode}')

    # Build project
    logger.info(f'Building CMake project...')
    process = subprocess.run([
        'cmake',
        '--build',
        str(output_dir)
    ], cwd=project_dir)
    if process.returncode != 0:
        raise RuntimeError(f'Command Failed! Exit Code = {process.returncode}')


def create_qmod(
        *,
        mod_json_path: str | Path,
        project_dir: str | Path,
        build_output_dir: str | Path
):
    mod_json_path = Path(mod_json_path).absolute()
    project_dir = Path(project_dir).absolute()
    build_output_dir = Path(build_output_dir).absolute()

    # Load mod.json
    mod_json_content: Optional[Dict[str, Any]] = None
    with open(mod_json_path) as file:
        mod_json_content = json.load(file)
    if not mod_json_content:
        raise RuntimeError('Bad JSON!')

    # Compile a list of all the files needed for the .qmod
    source_files: Set[Path] = {
        mod_json_path
    }

    # Cover image
    if value := mod_json_content.get('coverImage'):
        path = project_dir / value
        if path.exists():
            source_files.add(path)
        else:
            logger.warning(f'File does not exist: {path}')

    # Mod files
    if value := mod_json_content.get('modFiles'):
        for path in value:
            p = build_output_dir / path
            if p.exists():
                source_files.add(p)
            else:
                logger.warning(f'File does not exist: {p}')

    mod_id = mod_json_content['id']
    mod_version = mod_json_content['version']
    qmod_path = project_dir / f'{mod_id}-v{mod_version}.qmod'

    # Create .qmod file
    logger.info(f'Creating {qmod_path.relative_to(project_dir)}')
    with ZipFile(qmod_path, 'w', compression=ZIP_DEFLATED) as file:
        for path in source_files:
            logger.info(f'- {path.relative_to(project_dir)}')
            file.write(path, arcname=path.name)

    exit(1)


def main():
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--clean',
        action='store_true',
        help='Do a clean build'
    )
    parser.add_argument(
        '--build-type',
        '-t',
        choices=get_args(BuildType),
        default='debug'
    )
    args = parser.parse_args()

    # Assign arguments
    clean: bool = args.clean
    build_type: BuildType = args.build_type

    project_root_dir = (Path(__file__) / '..' / '..').resolve()
    build_output_dir = project_root_dir / 'build'

    # Clean build output
    if clean:
        logger.info(f'Cleaning build output at "{build_output_dir.absolute()}"')
        shutil.rmtree(build_output_dir)

    # Build
    build(
        project_dir=project_root_dir,
        output_dir=build_output_dir,
        build_type=build_type
    )

    # Create .qmod file
    create_qmod(
        mod_json_path=project_root_dir / 'mod.json',
        project_dir=project_root_dir,
        build_output_dir=build_output_dir
    )

    # Push mod files
    mod_json = None
    with open('../mod.json', 'r') as file:
        mod_json = json.load(file)
    mod_files = mod_json['modFiles']
    for mod_file in mod_files:
        process = subprocess.run([
            'adb',
            'push',
            str(Path('build', 'debug', mod_file)),
            str(Path('/sdcard/ModData/com.beatgames.beatsaber/Modloader/mods/').as_posix())
        ], cwd=project_root_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if process.returncode != 0:
            logger.error(f'Failed to push file to device: {mod_file}')
            logger.error(process.stdout.decode('utf-8'))
            return

    # Launch game
    stop_app(PACKAGE_NAME_BEAT_SABER)
    start_app(PACKAGE_NAME_BEAT_SABER)
    global is_app_running
    is_app_running = True

    # Start logging
    process = subprocess.run([
        'python',
        str(Path('tools/log.py')),
        '--exit-on-disconnect',
        '--pid',
        'com.beatgames.beatsaber',
        '--tag',
        'spotify-search'
    ], cwd=project_root_dir)
    if process.returncode != 0:
        logger.error(f'Logging Failed! Exit Code = {process.returncode}')
        return


if __name__ == '__main__':
    main()  # todo: auto exit if target proc dies
