import argparse
import re
import subprocess
import time
from enum import Enum
from pathlib import Path
from typing import List, Optional, Set, Union

ANSI_ESCAPE = '\x1B'
ANSI_COLOR_RESET = '[0m'

ANSI_COLOR_GRAY = '[90m'
ANSI_COLOR_YELLOW = '[33m'
ANSI_COLOR_RED = '[31m'
ANSI_COLOR_BRIGHT_MAGENTA = '[95m'

class Color(Enum):
    RESET = '[0m'
    RED = '[31m'
    YELLOW = '[33m'
    GRAY = '[90m'
    MAGENTA = '[95m'

    def wrap(self, content: str) -> str:
        return f'{ANSI_ESCAPE}{self.value}{content}{ANSI_ESCAPE}{Color.RESET.value}'

COLORS = {
    'D': ANSI_COLOR_GRAY,
    'W': ANSI_COLOR_YELLOW,
    'E': ANSI_COLOR_RED,
    'F': ANSI_COLOR_RED,
}

def wait_for_process(name: str) -> int:
    print(f'{ANSI_ESCAPE}{ANSI_COLOR_BRIGHT_MAGENTA}< Waiting for process: {name} >{ANSI_ESCAPE}{ANSI_COLOR_RESET}')
    while True:
        result = subprocess.run(
            ['adb', 'wait-for-device', 'shell', 'pidof', name],
            capture_output=True,
            text=True,
        )
        stdout = result.stdout.strip() if result.stdout else None
        if stdout:
            print(stdout)
            split = stdout.split(' ')
            if len(split) > 1:
                print('Found multiple PIDs')
                return int(split[-1])
            return int(stdout)
        time.sleep(1)


def main():
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('patterns', nargs='*', help='List of patterns to include.')
    parser.add_argument('--pid', type=str, help='PID or name of process')
    parser.add_argument('--exit-on-disconnect', action='store_true')
    parser.add_argument('--tag', action='append', default=[])
    args = parser.parse_args()

    patterns: List[str] = args.patterns
    target_pid_or_name = args.pid
    should_exit_on_device_disconnect: bool = args.exit_on_disconnect
    included_tags: List[str] = args.tag

    # Get included and excluded patterns
    included_patterns: Set[str] = set()
    excluded_patterns: Set[str] = set()
    for pattern in patterns:
        if pattern.startswith('-'):
            excluded_patterns.add(pattern[1:])
        else:
            included_patterns.add(pattern)

    excluded_regex_patterns: List[str] = [
        r'^Extracted invalid genIdx .+ from parameter \d+$',
        r'^Looking for: \d+, resolved to: \d+$',
        r'^After resolving method with slot: found method 0x.+$',
    ]

    try:
        while True:

            # Start the logging process
            process = subprocess.Popen(
                ['adb', 'wait-for-device', 'shell', 'logcat'],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                encoding='utf-8',
                errors='replace'
            )

            # Wait for the specified process to start
            target_pid = None
            if target_pid_or_name:
                target_pid = wait_for_process(target_pid_or_name)

            while True:

                # Read line
                line = process.stdout.readline().strip()

                # An empty line means we have reached EOF
                if not line:

                    # Wait for the process to end
                    while process.poll() is None:
                        time.sleep(0.1)
                    print(f'< Process Died With Exit Code {process.returncode} >')
                    break

                is_excluded = False
                for item in [
                    'Timestamp too new',
                    'equested timestamp from',
                    'BufferQueue has been abandoned',
                    'dequeueBuffer failed',
                    'potential method match had wrong number',
                    'is generic 0 is inflated 0',
                    'handling override method'
                ]:
                    if item.lower() in line.lower():
                        is_excluded = True
                        break

                if is_excluded:
                    continue

                # Check if the line matches the expected format
                match = re.match(r'^(\d+-\d+)\s(.+)\s+(\d+)\s+(\d+)\s(.)\s(.+?)\s*:(?:\s(.+))?$', line)
                if match:
                    date_string = match[1]
                    time_string = match[2]
                    process_id = int(match[3])
                    thread_id = int(match[4])
                    log_level = match[5]
                    log_tag = match[6]
                    log_message = match[7]

                    # Check if the log message matches the expected format
                    if log_message:
                        m = re.match(r'^\[(.+):(\d+):(\d+) @ (.+)] (.+)$', log_message)
                        if m:
                            source_file_path = Path(m[1])
                            source_file_line_number = m[2]
                            source_file_column_number = m[3]
                            source_file_function_name = m[4]
                            actual_log_message = m[5]
                            line = line.replace(m[1], source_file_path.name)

                            # Check excluded regex patterns
                            for pattern in excluded_regex_patterns:
                                match = re.fullmatch(pattern, actual_log_message)
                                if match:
                                    is_excluded = True
                                    break
                            if is_excluded:
                                continue

                    # Always show fatal logs
                    if log_level == 'F':
                        print(Color.RED.wrap(line))
                        continue

                    if len(included_tags) > 0:
                        is_included_tag = False
                        for tag in included_tags:
                            if log_tag == tag:
                                is_included_tag = True
                                break
                        if not is_included_tag:
                            continue

                    if log_tag in [
                        'VrApi',
                        'SongCore',
                        'PlaylistManager',
                        'BetterSongSearch',
                        'BSML'
                    ]:
                        continue

                    # Filter by PID
                    if target_pid:
                        if process_id != target_pid:
                            continue

                # If there are no filter patterns, skip the blow logic and just print the line
                if len(patterns) == 0:
                    print(line)
                    continue

                # Check for included patterns
                for pattern in included_patterns:
                    if pattern.lower() in line.lower():
                        print(line)

            if should_exit_on_device_disconnect:
                break
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
