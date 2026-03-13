#!/usr/bin/env python3
"""Download and filter COVID-19 data for simulation comparison.

Downloads OWID COVID-19 dataset and extracts South Korea first wave.
Output: data/covid_south_korea.csv
"""
import argparse
import csv
import os
import sys
import tempfile
import urllib.request

OWID_URL = "https://raw.githubusercontent.com/owid/covid-19-data/master/public/data/owid-covid-data.csv"
OUTPUT_DIR = "data"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "covid_south_korea.csv")

# South Korea first wave: Feb 18 - Apr 30, 2020
COUNTRY = "South Korea"
DATE_START = "2020-02-18"
DATE_END = "2020-04-30"

KEEP_COLS = [
    "date", "new_cases", "new_cases_smoothed", "total_cases",
    "new_deaths", "new_deaths_smoothed", "total_deaths", "population"
]


def fetch_and_filter(force=False):
    if os.path.exists(OUTPUT_FILE) and not force:
        print(f"Data file already exists: {OUTPUT_FILE}")
        print("Use --force to re-download.")
        return

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Download to temp file (~50MB)
    print(f"Downloading OWID COVID-19 dataset from {OWID_URL}...")
    tmp_fd, tmp_path = tempfile.mkstemp(suffix=".csv")
    try:
        urllib.request.urlretrieve(OWID_URL, tmp_path)
        print(f"Downloaded to temp file ({os.path.getsize(tmp_path) / 1e6:.1f} MB)")

        # Filter for South Korea + date range
        rows_written = 0
        with open(tmp_path, "r", newline="") as fin, \
             open(OUTPUT_FILE, "w", newline="") as fout:
            reader = csv.DictReader(fin)
            writer = csv.DictWriter(fout, fieldnames=KEEP_COLS)
            writer.writeheader()

            for row in reader:
                if row.get("location") != COUNTRY:
                    continue
                date = row.get("date", "")
                if date < DATE_START or date > DATE_END:
                    continue
                filtered = {}
                for col in KEEP_COLS:
                    filtered[col] = row.get(col, "")
                writer.writerow(filtered)
                rows_written += 1

        # Print summary
        print(f"\nFiltered data written to {OUTPUT_FILE}")
        print(f"  Rows: {rows_written}")
        print(f"  Date range: {DATE_START} to {DATE_END}")
        print(f"  Country: {COUNTRY}")

        # Quick stats from the filtered file
        with open(OUTPUT_FILE, "r") as f:
            reader = csv.DictReader(f)
            peak_new = 0
            total_cases = 0
            pop = 0
            for row in reader:
                try:
                    nc = float(row["new_cases"]) if row["new_cases"] else 0
                except ValueError:
                    nc = 0
                if nc > peak_new:
                    peak_new = nc
                try:
                    total_cases = int(float(row["total_cases"])) if row["total_cases"] else total_cases
                except ValueError:
                    pass
                try:
                    pop = int(float(row["population"])) if row["population"] else pop
                except ValueError:
                    pass
            print(f"  Peak daily new cases: {peak_new:.0f}")
            print(f"  Total cases (end of range): {total_cases}")
            if pop > 0:
                print(f"  Population: {pop:,}")
                print(f"  Infected fraction: {total_cases/pop*100:.4f}%")

    except urllib.error.URLError as e:
        print(f"Error downloading data: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        os.close(tmp_fd)
        os.unlink(tmp_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true",
                        help="Re-download even if data file exists")
    args = parser.parse_args()
    fetch_and_filter(force=args.force)


if __name__ == "__main__":
    main()
