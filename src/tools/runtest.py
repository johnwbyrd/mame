#!/usr/bin/env python3
"""
MAME Regression Testing Script
Python version of runtest.cmd/runtest.sh

Initial setup:
    1. Create a fresh directory mametest/
    2. Copy a mame.ini with your ROM paths into it (mametest/mame.ini)
    3. Copy a transparent crosshair cursor into it (mametest/cross.png)

How to run a test:
    1. Create a new subdirectory mametest/version/
    2. Copy mame executable into it (mametest/version/mame)
    3. Open a terminal to mametest/version
    4. Run: python3 ../../src/tools/runtest.py [optional_gamelist.txt]

How to generate a report:
    1. The script automatically combines worker CSV files into summary-final.csv
    2. Make sure you have run tests for at least two versions
    3. Create output directory: mkdir ../report
    4. Run: regrep ../report ver1/summary-final.csv ver2/summary-final.csv
"""

import argparse
import multiprocessing
import os
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False


def find_mame_executable():
    """Find the MAME executable in likely directories."""
    # Check current directory first
    for name in ['mame', 'mame.exe']:
        if os.path.isfile(name) and os.access(name, os.X_OK):
            return os.path.abspath(name)

    # Check parent directory (script might be in tools/)
    parent = Path('..')
    for name in ['mame', 'mame.exe']:
        candidate = parent / name
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate.absolute())

    # Check build directories (common MAME build locations)
    build_dirs = [
        '.',
        '..',
        '../..',
        'build/Release',
        'build/Debug',
        '../build/Release',
        '../build/Debug',
        '../../build/Release',
        '../../build/Debug',
    ]

    for build_dir in build_dirs:
        for name in ['mame', 'mame.exe', 'mame64', 'mame64.exe']:
            candidate = Path(build_dir) / name
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return str(candidate.absolute())

    # Check in PATH
    import shutil as sh
    mame_in_path = sh.which('mame')
    if mame_in_path:
        return mame_in_path

    return None


def generate_game_list(mame_exe, pattern=''):
    """Generate list of games using mame -ls."""
    cmd = [mame_exe, '-ls']
    if pattern:
        cmd.append(pattern)

    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stdout.splitlines()


def parse_game_line(line):
    """Parse a line from mame -ls output.
    Format: 'gamename        "source/path/file.cpp"'
    Returns: (gamename, source) or None
    """
    parts = line.split()
    if len(parts) < 2:
        return None

    gamename = parts[0]
    source = parts[1].strip('"')
    return (gamename, source)


def setup_directories(clean=False):
    """Create necessary directories and clean old data if requested."""
    if clean:
        for dirname in ['log', 'snap']:
            if os.path.isdir(dirname):
                print(f"Removing old {dirname}/ directory")
                shutil.rmtree(dirname)

    # Always clean these on every run
    for dirname in ['cfg', 'nvram', 'diff']:
        if os.path.isdir(dirname):
            shutil.rmtree(dirname)

    # Create necessary directories
    os.makedirs('log', exist_ok=True)
    os.makedirs('artwork', exist_ok=True)


def setup_crosshairs():
    """Copy transparent crosshair cursors."""
    cross_src = Path('..') / 'cross.png'
    if cross_src.exists():
        for i in range(4):
            shutil.copy(cross_src, f'artwork/cross{i}.png')


def create_summary_header(mame_exe, summary_file):
    """Create initial summary.log with MAME version info."""
    if os.path.exists(summary_file):
        return

    result = subprocess.run([mame_exe, '-help'], capture_output=True, text=True)

    with open(summary_file, 'w') as f:
        f.write(result.stdout)
        f.write(f"@@@@@dir={os.getcwd()}\n")


def run_one_test(mame_exe, gamename, source, summary_file):
    """Run a single game test for 10 seconds and write CSV result."""
    print(f"Testing {gamename} ({source})...")

    log_stdout = Path('log') / f'{gamename}.txt'
    log_stderr = Path('log') / f'{gamename}.err'

    # Run MAME
    cmd = [
        mame_exe, gamename,
        '-seconds_to_run', '5',
        '-watchdog', '30',
        '-nodebug',
        '-nothrottle',
        '-inipath', '..',
        '-window',
        '-video', 'none',
        '-sound', 'none'
    ]

    # Set up environment to use SDL dummy video driver (prevents window creation)
    env = os.environ.copy()
    env['SDL_VIDEODRIVER'] = 'dummy'

    with open(log_stdout, 'w') as fout, open(log_stderr, 'w') as ferr:
        proc = subprocess.Popen(cmd, stdout=fout, stderr=ferr, env=env)

        # Set process priority to lowest if psutil is available
        if HAS_PSUTIL:
            try:
                p = psutil.Process(proc.pid)
                if sys.platform == 'win32':
                    p.nice(psutil.IDLE_PRIORITY_CLASS)
                else:
                    p.nice(19)  # Lowest priority on Unix
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass  # Process may have already finished or we lack permissions

        proc.wait()
        result = proc

    # Process exit code
    exit_code = result.returncode

    # Map exit code to status
    if exit_code == 100:
        status = "Exception"
    elif exit_code == 5:
        status = "Not in build"
    elif exit_code == 3:
        status = "Fatal error"
    elif exit_code == 2:
        status = "Missing files"
    elif exit_code == 1:
        status = "Failed validity"
    elif exit_code == 0:
        status = "Success"
    else:
        status = f"Unknown error {exit_code}"

    # Write CSV result to worker's summary file
    with open(summary_file, 'a') as f:
        f.write(f"{gamename},{source},{status},{exit_code}\n")


def worker_process(worker_id, game_list, mame_exe, num_workers):
    """Worker process that tests games assigned to it."""
    # Create this worker's summary CSV file
    summary_file = f'summary{worker_id}.csv'

    # Write CSV header
    with open(summary_file, 'w') as f:
        f.write('gamename,source,status,exit_code\n')

    # Process games assigned to this worker (round-robin)
    for idx, game_info in enumerate(game_list):
        if idx % num_workers != worker_id:
            continue

        parsed = parse_game_line(game_info)
        if not parsed:
            continue

        gamename, source = parsed
        run_one_test(mame_exe, gamename, source, summary_file)


def main():
    parser = argparse.ArgumentParser(
        description='MAME regression testing tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('gamelist', nargs='?', help='Optional file with game list (from mame -ls)')
    parser.add_argument('-j', '--jobs', type=int, help='Number of parallel workers (default: CPU count)')
    parser.add_argument('-p', '--pattern', help='Game pattern for mame -ls (e.g., "zbc*")')

    args = parser.parse_args()

    # Find MAME executable
    mame_exe = find_mame_executable()
    if not mame_exe:
        print("ERROR: Cannot find mame executable in current directory!", file=sys.stderr)
        print("Please run this script from the directory containing your mame executable.", file=sys.stderr)
        return 1

    print(f"Found MAME executable: {mame_exe}")

    # Determine number of workers (default to half of available CPUs)
    num_workers = args.jobs or (multiprocessing.cpu_count() // 2)
    print(f"Using {num_workers} parallel workers")

    # Generate or load game list
    if args.gamelist:
        # Check if it's a file or a pattern
        if os.path.isfile(args.gamelist):
            print(f"Loading game list from {args.gamelist}")
            with open(args.gamelist, 'r') as f:
                game_list = [line.strip() for line in f if line.strip()]
            setup_directories(clean=False)
        else:
            # Treat as a pattern
            print(f"Treating '{args.gamelist}' as pattern (file not found)")
            game_list = generate_game_list(mame_exe, args.gamelist)
            setup_directories(clean=True)
    else:
        pattern = args.pattern or ''
        print(f"Generating game list" + (f" for pattern '{pattern}'" if pattern else ""))
        game_list = generate_game_list(mame_exe, pattern)
        setup_directories(clean=True)

    print(f"Found {len(game_list)} games to test")

    # Sort the game list alphabetically
    game_list.sort()

    # Setup
    setup_crosshairs()

    # Launch worker processes
    print("Launching workers...")
    processes = []
    for worker_id in range(num_workers):
        p = multiprocessing.Process(
            target=worker_process,
            args=(worker_id, game_list, mame_exe, num_workers)
        )
        p.start()
        processes.append(p)
        print(f"  Worker {worker_id} started (PID {p.pid})")

    # Wait for all workers to complete
    print(f"\nWaiting for {num_workers} workers to complete...")
    for p in processes:
        p.join()

    print("\nAll tests completed!")
    print("Combining worker CSV files...")

    # Read all worker summary CSV files and combine them
    import glob
    import csv

    results = []
    summary_files = glob.glob('summary*.csv')

    for summary_file in summary_files:
        with open(summary_file, 'r') as f:
            # Skip header line
            reader = csv.reader(f)
            next(reader, None)  # Skip header
            for row in reader:
                if row:  # Skip empty lines
                    results.append(','.join(row))

    # Sort results by gamename (first field)
    results.sort(key=lambda x: x.split(',')[0])

    # Write sorted CSV
    with open('summary-final.csv', 'w') as f:
        f.write('gamename,source,status,exit_code\n')
        for result in results:
            f.write(result + '\n')

    print(f"Combined {len(summary_files)} worker files into summary-final.csv")
    print(f"Total results: {len(results)}")
    print(f"\nResults saved in: summary-final.csv")
    print(f"Worker files: {', '.join(sorted(summary_files))}")
    print(f"Logs saved in: log/")

    return 0


if __name__ == '__main__':
    sys.exit(main())
