#!/usr/bin/env python3
"""
Update zbc_status.csv with test results from summary-final.csv

Reads test results and marks machines as working or broken based on actual runtime behavior.
"""

import csv
import sys
from pathlib import Path


def read_test_results(summary_csv_path):
    """Read test results and return dict mapping shortname -> status."""
    results = {}

    with open(summary_csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            gamename = row['gamename']
            status = row['status']
            exit_code = int(row['exit_code'])

            # Strip 'zbc' prefix to get CPU shortname
            if gamename.startswith('zbc'):
                shortname = gamename[3:]  # Remove 'zbc' prefix
            else:
                shortname = gamename

            # Map test status to zbc_status value
            if status == 'Success' and exit_code == 0:
                zbc_status = 'working'
            else:
                # Any failure gets marked as broken_validate
                zbc_status = 'broken_validate'

            # Store the result (use first occurrence if duplicates)
            if shortname not in results:
                results[shortname] = {
                    'status': zbc_status,
                    'test_status': status,
                    'exit_code': exit_code
                }

    return results


def update_status_csv(status_csv_path, test_results):
    """Update zbc_status.csv with test results."""
    rows = []
    changes = []

    # Read existing CSV
    with open(status_csv_path, 'r') as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames

        for row in reader:
            shortname = row['shortname']
            old_status = row['status']

            # Check if we have test results for this CPU
            if shortname in test_results:
                result = test_results[shortname]
                new_status = result['status']

                # Only update if status changed
                if old_status != new_status:
                    changes.append({
                        'shortname': shortname,
                        'old_status': old_status,
                        'new_status': new_status,
                        'test_status': result['test_status'],
                        'exit_code': result['exit_code']
                    })
                    row['status'] = new_status

                    # Update notes if it's a failure
                    if new_status == 'broken_validate':
                        note = f"Test failed: {result['test_status']} (exit {result['exit_code']})"
                        row['notes'] = note
                    elif new_status == 'working' and 'Test failed' in row['notes']:
                        # Clear failure note if it's now working
                        row['notes'] = ''

            rows.append(row)

    # Write updated CSV
    with open(status_csv_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    return changes


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 update_zbc_status.py <summary-final.csv> <zbc_status.csv>")
        print()
        print("Example:")
        print("  python3 update_zbc_status.py mametest/v1/summary-final.csv src/mame/zbc/zbc_status.csv")
        return 1

    summary_csv = Path(sys.argv[1])
    status_csv = Path(sys.argv[2])

    if not summary_csv.exists():
        print(f"ERROR: Test results file not found: {summary_csv}")
        return 1

    if not status_csv.exists():
        print(f"ERROR: Status CSV file not found: {status_csv}")
        return 1

    print(f"Reading test results from: {summary_csv}")
    test_results = read_test_results(summary_csv)
    print(f"Found test results for {len(test_results)} machines")

    print(f"\nUpdating status CSV: {status_csv}")
    changes = update_status_csv(status_csv, test_results)

    if changes:
        print(f"\nStatus changes ({len(changes)} machines):")
        print()

        # Group by change type
        to_broken = [c for c in changes if c['new_status'] == 'broken_validate']
        to_working = [c for c in changes if c['new_status'] == 'working']

        if to_broken:
            print(f"Changed to BROKEN ({len(to_broken)}):")
            for change in to_broken:
                print(f"  {change['shortname']:20} | {change['old_status']:20} → {change['new_status']:20} | {change['test_status']}")

        if to_working:
            print(f"\nChanged to WORKING ({len(to_working)}):")
            for change in to_working:
                print(f"  {change['shortname']:20} | {change['old_status']:20} → {change['new_status']:20}")
    else:
        print("\nNo status changes needed - all machines match their test results")

    # Summary statistics
    working_count = sum(1 for r in test_results.values() if r['status'] == 'working')
    broken_count = len(test_results) - working_count

    print(f"\nTest Results Summary:")
    print(f"  Working machines: {working_count}")
    print(f"  Broken machines:  {broken_count}")
    print(f"  Total tested:     {len(test_results)}")

    return 0


if __name__ == '__main__':
    sys.exit(main())
