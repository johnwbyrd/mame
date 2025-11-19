#!/usr/bin/python3
##
## license:BSD-3-Clause
## copyright-holders:John Byrd
"""
ZBC CPU Database Manager

The ZBC (Zero Board Computer) system provides minimal test environments for
all CPU architectures in MAME. This tool manages the CPU database (CSV file)
and generates C++ source code.

THE CSV FILE IS THE SOURCE OF TRUTH (src/mame/zbc/zbc_status.csv):
  - Tracks which CPUs work, which are broken, and why
  - Version controlled to preserve knowledge over time
  - Edit manually or update with commands below

WORKFLOW:
  1. Scan MAME to discover CPUs    (updates CSV)
  2. Mark broken CPUs from logs    (updates CSV)
  3. Build source files from CSV   (generates code)

See --help for complete usage information.
"""

import argparse
import csv
import re
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from collections import defaultdict

# Status constants
STATUS_WORKING = "working"
STATUS_BROKEN_COMPILE = "broken_compile"
STATUS_BROKEN_VALIDATE = "broken_validate"
STATUS_BROKEN_HEADER = "broken_header"
STATUS_DISABLED = "disabled"
STATUS_NOT_CPU = "not_cpu"
STATUS_NEEDS_INTERNAL_ROM = "needs_internal_rom"
STATUS_NEEDS_DEPENDENT_DEVICE = "needs_dependent_device"
STATUS_UNKNOWN = "unknown"

ALL_STATUSES = [STATUS_WORKING, STATUS_BROKEN_COMPILE, STATUS_BROKEN_VALIDATE,
                STATUS_BROKEN_HEADER, STATUS_DISABLED, STATUS_NOT_CPU,
                STATUS_NEEDS_INTERNAL_ROM, STATUS_NEEDS_DEPENDENT_DEVICE, STATUS_UNKNOWN]


class CPUInfo:
    """Represents a single CPU device with all metadata"""

    def __init__(self, shortname: str, type_constant: str, class_name: str,
                 fullname: str, status: str = STATUS_UNKNOWN,
                 header_file: str = "", notes: str = ""):
        self.shortname = shortname
        self.type_constant = type_constant
        self.class_name = class_name
        self.fullname = fullname
        self.status = status
        self.header_file = header_file
        self.notes = notes

    def to_csv_row(self) -> List[str]:
        """Convert to CSV row"""
        return [self.shortname, self.type_constant, self.class_name,
                self.fullname, self.status, self.header_file, self.notes]

    @staticmethod
    def from_csv_row(row: List[str]) -> 'CPUInfo':
        """Create from CSV row"""
        # Handle rows with missing fields
        while len(row) < 7:
            row.append("")
        return CPUInfo(*row[:7])

    @staticmethod
    def csv_header() -> List[str]:
        """Get CSV header row"""
        return ["shortname", "type_constant", "class_name", "fullname",
                "status", "header_file", "notes"]


class ZBCGenerator:
    """Main generator class for ZBC system"""

    def __init__(self, csv_path: str = "src/mame/zbc/zbc_status.csv"):
        self.csv_path = Path(csv_path)
        self.cpus: Dict[str, CPUInfo] = {}
        self.device_type_mapping: Dict[str, str] = {}  # TYPE_CONSTANT -> header_path

    def build_device_type_mapping(self) -> Dict[str, str]:
        """
        Scan all CPU header files and build mapping from DECLARE_DEVICE_TYPE to header paths.
        Loads ALL headers into memory once, then searches in-memory for efficiency.
        Returns: Dict[TYPE_CONSTANT, header_path]
        """
        cpu_root = Path("src/devices/cpu")
        if not cpu_root.exists():
            print(f"Warning: {cpu_root} not found, cannot build device type mapping")
            return {}

        # Step 1: Load ALL header files into memory at once (~6.3 MB total)
        print(f"Loading all CPU headers into memory...")
        headers: Dict[Path, str] = {}
        for path in cpu_root.rglob("*.h"):
            try:
                headers[path] = path.read_text(encoding='utf-8', errors='ignore')
            except Exception as e:
                print(f"  Warning: Could not read {path}: {e}")

        print(f"Loaded {len(headers)} header files into memory")

        # Step 2: Search ALL files in memory for DECLARE_DEVICE_TYPE
        pattern = re.compile(r'DECLARE_DEVICE_TYPE\s*\(\s*(\w+)\s*,')
        mapping: Dict[str, str] = {}

        for path, content in headers.items():
            for match in pattern.finditer(content):
                type_constant = match.group(1)
                try:
                    rel_path = path.relative_to("src/devices")
                    mapping[type_constant] = str(rel_path)
                except ValueError:
                    # Path not relative to src/devices, use absolute
                    mapping[type_constant] = str(path)

        print(f"Found {len(mapping)} DECLARE_DEVICE_TYPE declarations")
        self.device_type_mapping = mapping
        return mapping

    def load_csv(self) -> bool:
        """Load CPU database from CSV file. Returns True if loaded, False if not found."""
        if not self.csv_path.exists():
            return False

        with open(self.csv_path, 'r', newline='', encoding='utf-8') as f:
            reader = csv.reader(f)
            header = next(reader, None)
            if header != CPUInfo.csv_header():
                print(f"Warning: CSV header mismatch")

            for row in reader:
                if not row or row[0].startswith('#'):
                    continue  # Skip empty lines and comments
                cpu = CPUInfo.from_csv_row(row)
                self.cpus[cpu.shortname] = cpu

        print(f"Loaded {len(self.cpus)} CPUs from {self.csv_path}")
        return True

    def save_csv(self) -> None:
        """Save CPU database to CSV file"""
        self.csv_path.parent.mkdir(parents=True, exist_ok=True)

        with open(self.csv_path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(CPUInfo.csv_header())

            # Sort by shortname for consistent output
            for shortname in sorted(self.cpus.keys()):
                writer.writerow(self.cpus[shortname].to_csv_row())

        print(f"Saved {len(self.cpus)} CPUs to {self.csv_path}")

    def scan_mame(self, listcpu_file: str) -> None:
        """
        Scan MAME's -listcpu output and update CSV database.
        PRESERVES existing CPU status - only updates metadata for existing CPUs.
        """
        # Load existing database first (if exists)
        csv_existed = self.load_csv()

        with open(listcpu_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Detect format (GCC/Clang has 4 columns, MSVC has 3)
        header_line = lines[0] if lines else ""
        has_class_column = "Device class:" in header_line

        print(f"Scanning MAME -listcpu output ({4 if has_class_column else 3} column format)")

        added = 0
        updated = 0

        for line in lines[1:]:  # Skip header
            line = line.strip()
            if not line:
                continue

            # Parse based on format
            if has_class_column:
                # 4 columns: shortname type_constant class_name "fullname"
                match = re.match(r'^(\S+)\s+(\S+)\s+(\S+)\s+"([^"]+)"', line)
                if match:
                    shortname, type_const, class_name, fullname = match.groups()
                else:
                    continue
            else:
                # 3 columns: shortname type_constant "fullname"
                match = re.match(r'^(\S+)\s+(\S+)\s+"([^"]+)"', line)
                if match:
                    shortname, type_const, fullname = match.groups()
                    class_name = ""  # MSVC doesn't provide class name
                else:
                    continue

            # Update or add CPU
            if shortname in self.cpus:
                # PRESERVE: status, notes
                # UPDATE: type_constant, class_name, fullname, header_file
                cpu = self.cpus[shortname]
                cpu.type_constant = type_const
                if class_name:
                    cpu.class_name = class_name
                cpu.fullname = fullname
                cpu.header_file = self.infer_header_file(cpu)
                updated += 1
            else:
                # NEW: Infer header and set appropriate initial status
                header_file = self.infer_header_file(CPUInfo(shortname, type_const, class_name, fullname, "", "", ""))
                if not header_file:
                    # Header not found in mapping - mark as broken_header
                    status = STATUS_BROKEN_HEADER
                else:
                    # Header found - mark as working (will be validated later)
                    status = STATUS_WORKING

                cpu = CPUInfo(shortname, type_const, class_name, fullname, status, header_file, "")
                self.cpus[shortname] = cpu
                added += 1

        if csv_existed:
            print(f"Added {added} new CPUs, updated {updated} existing CPUs (preserved status)")
        else:
            print(f"Created new database with {added} CPUs")

    def infer_header_file(self, cpu: CPUInfo) -> str:
        """Infer header file path by looking up type constant in scanned device type mapping"""
        if not cpu.type_constant:
            return ""

        # Build mapping on first use
        if not self.device_type_mapping:
            self.build_device_type_mapping()

        # Look up the actual header path from scanned DECLARE_DEVICE_TYPE declarations
        if cpu.type_constant in self.device_type_mapping:
            return self.device_type_mapping[cpu.type_constant]

        # Fallback: if not found in mapping, return empty (will be caught by validation)
        return ""

    def mark_broken_compile(self, build_log: str) -> None:
        """Parse build error log and mark broken CPUs as broken_compile"""
        with open(build_log, 'r', encoding='utf-8', errors='ignore') as f:
            log_content = f.read()

        # Patterns for compilation errors
        # GCC/Clang: "file.cpp:123:45: error: message"
        # MSVC: "file.cpp(123): error C2065: message"

        gcc_pattern = r'zbcgen\.cpp:(\d+):\d+:\s+error:\s+(.+)'
        msvc_pattern = r'zbcgen\.cpp\((\d+)\):\s+error\s+\w+:\s+(.+)'

        errors_found = 0

        for match in re.finditer(gcc_pattern, log_content):
            line_num, error_msg = match.groups()
            errors_found += 1
            # TODO: Map line number to CPU and mark as broken
            print(f"Error at line {line_num}: {error_msg[:60]}...")

        for match in re.finditer(msvc_pattern, log_content):
            line_num, error_msg = match.groups()
            errors_found += 1
            print(f"Error at line {line_num}: {error_msg[:60]}...")

        if errors_found == 0:
            print("No compilation errors found in build log")
        else:
            print(f"Found {errors_found} compilation errors (line mapping not yet implemented)")

    def mark_broken_validate(self, validate_log: str) -> None:
        """Parse validation error log and mark broken CPUs as broken_validate"""
        with open(validate_log, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        # Pattern: "Driver zbcXXX (file zbc.cpp): N errors, M warnings"
        pattern = r'Driver\s+(zbc\w+)\s+\(file\s+\w+\.cpp\):\s+(\d+)\s+error'

        marked = 0

        for line in lines:
            match = re.search(pattern, line)
            if match:
                driver_name, error_count = match.groups()
                # Extract shortname (remove "zbc" prefix)
                shortname = driver_name[3:]  # Remove "zbc"

                if shortname in self.cpus and int(error_count) > 0:
                    cpu = self.cpus[shortname]
                    if cpu.status == STATUS_WORKING:
                        cpu.status = STATUS_BROKEN_VALIDATE
                        cpu.notes = f"Validation failed with {error_count} errors"
                        marked += 1
                        print(f"Marked {shortname} as broken_validate")

        if marked == 0:
            print("No validation errors found in log")
        else:
            print(f"Marked {marked} CPUs as broken_validate")

    def validate_header_and_device(self, header_path: str, type_constant: str) -> bool:
        """Check if header file exists AND declares the device type"""
        # Try relative to MAME source root
        full_path = Path("src/devices") / header_path

        if not full_path.exists():
            return False

        # Check if the header actually declares this device type
        try:
            content = full_path.read_text(encoding='utf-8', errors='ignore')
            # Look for DECLARE_DEVICE_TYPE(TYPE_CONSTANT, ...)
            pattern = rf'DECLARE_DEVICE_TYPE\s*\(\s*{re.escape(type_constant)}\s*,'
            if re.search(pattern, content):
                return True
        except Exception:
            pass

        return False

    def generate_hpp(self, output_path: str = "src/mame/zbc/zbcgen.hpp") -> List[CPUInfo]:
        """Generate zbcgen.hpp with all CPU header includes"""
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)

        # Collect headers (only for working CPUs with valid headers)
        headers = set()
        missing_headers = []
        cpus_with_missing_headers = []

        for cpu in self.cpus.values():
            if cpu.status == STATUS_WORKING and cpu.header_file:
                if self.validate_header_and_device(cpu.header_file, cpu.type_constant):
                    headers.add(cpu.header_file)
                else:
                    missing_headers.append((cpu.shortname, cpu.header_file))
                    cpus_with_missing_headers.append(cpu)

        # Sort alphabetically
        sorted_headers = sorted(headers)

        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("// Auto-generated by zbcgen.py - DO NOT EDIT\n")
            f.write(f"// Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"// Source: {self.csv_path}\n")
            f.write(f"// Working CPUs: {len([c for c in self.cpus.values() if c.status == STATUS_WORKING])}\n")
            if missing_headers:
                f.write(f"// WARNING: {len(missing_headers)} CPUs have missing header files (excluded)\n")
            f.write("\n")
            f.write("#ifndef MAME_ZBC_ZBCGEN_HPP\n")
            f.write("#define MAME_ZBC_ZBCGEN_HPP\n")
            f.write("\n")
            f.write("// CPU headers (alphabetically sorted)\n")

            for header in sorted_headers:
                f.write(f'#include "{header}"\n')

            f.write("\n")
            f.write("#endif // MAME_ZBC_ZBCGEN_HPP\n")

        print(f"Generated {output_file} with {len(sorted_headers)} headers")

        if missing_headers:
            print(f"\nWARNING: {len(missing_headers)} CPUs have missing header files:")
            for shortname, header in missing_headers[:10]:
                print(f"  - {shortname}: {header}")
            if len(missing_headers) > 10:
                print(f"  ... and {len(missing_headers) - 10} more")

        return cpus_with_missing_headers

    def generate_ipp(self, output_path: str = "src/mame/zbc/zbcgen.ipp", cpus_with_missing_headers: Optional[List[CPUInfo]] = None) -> None:
        """Generate zbcgen.ipp with all DEFINE_ZBC calls (NO #includes - will be included by zbc.cpp)"""
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)

        if cpus_with_missing_headers is None:
            cpus_with_missing_headers = []

        # Build set of CPUs with missing headers for quick lookup
        missing_header_shortnames = {cpu.shortname for cpu in cpus_with_missing_headers}

        # Count statistics
        stats = defaultdict(int)
        for cpu in self.cpus.values():
            stats[cpu.status] += 1

        with open(output_file, 'w', encoding='utf-8') as f:
            # Header (NO #includes - this file will be #included by zbc.cpp)
            f.write("// Auto-generated by zbcgen.py - DO NOT EDIT\n")
            f.write(f"// Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"// Source: {self.csv_path}\n")
            f.write("//\n")
            f.write("// This file is #included by zbc.cpp, not compiled separately.\n")
            f.write("// Do not add #include directives here.\n")
            f.write("//\n")
            f.write("// Statistics:\n")
            f.write(f"//   Total CPUs discovered: {len(self.cpus)}\n")
            for status in ALL_STATUSES:
                f.write(f"//   {status}: {stats[status]}\n")
            if cpus_with_missing_headers:
                f.write(f"//   Missing headers: {len(cpus_with_missing_headers)}\n")
            f.write("\n")

            # Generate DEFINE_ZBC calls
            # Group by status (but exclude CPUs with missing headers from working)
            working_cpus = [c for c in self.cpus.values()
                           if c.status == STATUS_WORKING and c.shortname not in missing_header_shortnames]
            broken_cpus = [c for c in self.cpus.values() if c.status != STATUS_WORKING]

            # Working CPUs
            f.write(f"// Working CPUs ({len(working_cpus)})\n")
            f.write("// These CPUs compile and validate successfully\n")
            f.write("\n")

            for cpu in sorted(working_cpus, key=lambda c: c.shortname):
                if not cpu.class_name or not cpu.type_constant:
                    continue
                f.write(f'DEFINE_ZBC({cpu.class_name}, {cpu.type_constant}, '
                       f'{cpu.shortname}, "{cpu.fullname}")\n')

            f.write("\n")

            # CPUs with missing headers (commented out)
            if cpus_with_missing_headers:
                f.write(f"// CPUs with Missing Headers ({len(cpus_with_missing_headers)})\n")
                f.write("// These CPUs are commented out because their header files could not be found\n")
                f.write("// Fix the header_file path in zbc_status.csv and regenerate\n")
                f.write("\n")

                for cpu in sorted(cpus_with_missing_headers, key=lambda c: c.shortname):
                    if not cpu.class_name or not cpu.type_constant:
                        continue
                    f.write(f'// DEFINE_ZBC({cpu.class_name}, {cpu.type_constant}, '
                           f'{cpu.shortname}, "{cpu.fullname}")\n')
                    f.write(f'//   Header not found: {cpu.header_file}\n')
                    f.write("\n")

            # Broken/disabled CPUs (commented out)
            f.write(f"// Broken/Disabled CPUs ({len(broken_cpus)})\n")
            f.write("// These CPUs are commented out due to compilation or validation issues\n")
            f.write("\n")

            for cpu in sorted(broken_cpus, key=lambda c: c.shortname):
                if not cpu.class_name or not cpu.type_constant:
                    continue
                f.write(f'// DEFINE_ZBC({cpu.class_name}, {cpu.type_constant}, '
                       f'{cpu.shortname}, "{cpu.fullname}")\n')
                f.write(f'//   Status: {cpu.status}\n')
                if cpu.notes:
                    f.write(f'//   Notes: {cpu.notes}\n')
                f.write("\n")

        print(f"Generated {output_file} with {len(working_cpus)} working CPUs")
        if cpus_with_missing_headers:
            print(f"  ({len(cpus_with_missing_headers)} CPUs excluded due to missing headers)")

    def update_mame_lst(self, lst_path: str = "src/mame/mame.lst", cpus_with_valid_headers: list = None) -> None:
        """Update mame.lst with only CPUs that have valid headers"""
        lst_file = Path(lst_path)

        if not lst_file.exists():
            print(f"Warning: {lst_file} not found, skipping mame.lst update")
            return

        # Build set of CPUs with invalid headers
        if cpus_with_valid_headers is not None:
            valid_shortnames = {cpu.shortname for cpu in cpus_with_valid_headers}
        else:
            valid_shortnames = set()

        # Read entire file
        with open(lst_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Find @source:zbc/zbc.cpp section
        start_idx = None
        end_idx = None

        for i, line in enumerate(lines):
            if '@source:zbc/zbc.cpp' in line:
                start_idx = i
            elif start_idx is not None and line.startswith('@source:'):
                end_idx = i
                break

        if start_idx is None:
            print("Warning: @source:zbc/zbc.cpp not found in mame.lst")
            return

        if end_idx is None:
            end_idx = len(lines)

        # Generate new zbc section - only include CPUs with valid headers
        if cpus_with_valid_headers is not None:
            working_cpus = sorted(cpus_with_valid_headers, key=lambda c: c.shortname)
        else:
            working_cpus = sorted([c for c in self.cpus.values() if c.status == STATUS_WORKING],
                                 key=lambda c: c.shortname)

        new_lines = ["@source:zbc/zbc.cpp\n"]
        for cpu in working_cpus:
            new_lines.append(f"zbc{cpu.shortname}\n")
        new_lines.append("\n")

        # Replace section
        lines[start_idx:end_idx] = new_lines

        # Write back
        with open(lst_file, 'w', encoding='utf-8') as f:
            f.writelines(lines)

        print(f"Updated {lst_file} with {len(working_cpus)} ZBC drivers")


def main():
    parser = argparse.ArgumentParser(
        description='ZBC CPU Database Manager',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
THE CSV FILE IS THE SOURCE OF TRUTH:
  - Tracks which CPUs work, which are broken, and why
  - Version controlled to preserve knowledge over time
  - Edit manually or update with commands below

WORKFLOW:
  1. Scan MAME to discover CPUs    (updates CSV)
  2. Mark broken CPUs from logs    (updates CSV)
  3. Build source files from CSV   (generates code)

EXAMPLES:

  Initial setup:
    ./mame -listcpu > /tmp/cpus.txt
    %(prog)s --scan-mame /tmp/cpus.txt
    # Edit CSV to mark known working CPUs as "working"
    %(prog)s --build

  After build failures:
    make 2>&1 | %(prog)s --mark-broken-compile /dev/stdin
    %(prog)s --build
    make  # Retry without broken CPUs

  After validation failures:
    ./mame -validate 2>&1 | %(prog)s --mark-broken-validate /dev/stdin
    %(prog)s --build

  After MAME update (preserves existing CPU status!):
    ./mame -listcpu | %(prog)s --scan-mame /dev/stdin --build

For detailed documentation, see docs/source/techspecs/zbc.rst
        """)

    parser.add_argument('--csv', default='src/mame/zbc/zbc_status.csv',
                       help='Path to CSV database (default: src/mame/zbc/zbc_status.csv)')

    parser.add_argument('--scan-mame', metavar='LISTCPU_FILE',
                       help='Scan MAME CPU list and update CSV (preserves existing status)')

    parser.add_argument('--mark-broken-compile', metavar='BUILD_LOG',
                       help='Parse build errors and mark CPUs as broken_compile')

    parser.add_argument('--mark-broken-validate', metavar='VALIDATE_LOG',
                       help='Parse validation errors and mark CPUs as broken_validate')

    parser.add_argument('--build', action='store_true',
                       help='Generate zbcgen.hpp, zbcgen.cpp, and update mame.lst from CSV')

    args = parser.parse_args()

    # Create generator
    gen = ZBCGenerator(args.csv)

    # Process commands
    csv_modified = False

    if args.scan_mame:
        gen.scan_mame(args.scan_mame)
        csv_modified = True

    if args.mark_broken_compile:
        gen.load_csv()
        gen.mark_broken_compile(args.mark_broken_compile)
        csv_modified = True

    if args.mark_broken_validate:
        gen.load_csv()
        gen.mark_broken_validate(args.mark_broken_validate)
        csv_modified = True

    # Save CSV if modified
    if csv_modified:
        gen.save_csv()

    # Generate output files
    if args.build:
        if not gen.cpus:  # Not loaded yet
            if not gen.load_csv():
                print(f"Error: CSV file {args.csv} not found. Run --scan-mame first.")
                return 1

        working_count = len([c for c in gen.cpus.values() if c.status == STATUS_WORKING])
        if working_count == 0:
            print(f"Warning: No CPUs marked as 'working' in CSV. Generated files will be empty.")
            print(f"Edit {args.csv} to mark CPUs as 'working', or run --scan-mame.")

        cpus_with_missing_headers = gen.generate_hpp()
        gen.generate_ipp(cpus_with_missing_headers=cpus_with_missing_headers)

        # Calculate which CPUs have valid headers (working minus missing)
        missing_shortnames = {cpu.shortname for cpu in cpus_with_missing_headers}
        cpus_with_valid_headers = [c for c in gen.cpus.values()
                                   if c.status == STATUS_WORKING and c.shortname not in missing_shortnames]
        gen.update_mame_lst(cpus_with_valid_headers=cpus_with_valid_headers)

    if not any([args.scan_mame, args.mark_broken_compile, args.mark_broken_validate, args.build]):
        parser.print_help()
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
