#!/usr/bin/env python3
"""Download and filter COVID-19 data for simulation comparison.

Downloads OWID COVID-19 dataset and extracts a country's first wave.
Default: Italy first wave (Feb 21 – May 18, 2020).

Usage:
    python3 scripts/fetch_covid_data.py --force
    python3 scripts/fetch_covid_data.py --country "South Korea" --start 2020-02-18 --end 2020-04-30
"""
import argparse
import csv
import os
import sys
import tempfile
import urllib.request

OWID_URL = "https://raw.githubusercontent.com/owid/covid-19-data/master/public/data/owid-covid-data.csv"
OUTPUT_DIR = "data"

KEEP_COLS = [
    "date", "new_cases", "new_cases_smoothed", "total_cases",
    "new_deaths", "new_deaths_smoothed", "total_deaths", "population"
]


def country_slug(name):
    """Convert country name to filesystem-safe slug."""
    return name.lower().replace(" ", "_")


def fetch_and_filter(country, date_start, date_end, force=False):
    slug = country_slug(country)
    output_file = os.path.join(OUTPUT_DIR, f"covid_{slug}.csv")

    if os.path.exists(output_file) and not force:
        print(f"Data file already exists: {output_file}")
        print("Use --force to re-download.")
        return

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print(f"Downloading OWID COVID-19 dataset from {OWID_URL}...")
    tmp_fd, tmp_path = tempfile.mkstemp(suffix=".csv")
    try:
        urllib.request.urlretrieve(OWID_URL, tmp_path)
        print(f"Downloaded to temp file ({os.path.getsize(tmp_path) / 1e6:.1f} MB)")

        rows_written = 0
        with open(tmp_path, "r", newline="") as fin, \
             open(output_file, "w", newline="") as fout:
            reader = csv.DictReader(fin)
            writer = csv.DictWriter(fout, fieldnames=KEEP_COLS)
            writer.writeheader()

            for row in reader:
                if row.get("location") != country:
                    continue
                date = row.get("date", "")
                if date < date_start or date > date_end:
                    continue
                filtered = {col: row.get(col, "") for col in KEEP_COLS}
                writer.writerow(filtered)
                rows_written += 1

        print(f"\nFiltered data written to {output_file}")
        print(f"  Rows: {rows_written}")
        print(f"  Date range: {date_start} to {date_end}")
        print(f"  Country: {country}")

        # Summary stats
        with open(output_file, "r") as f:
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
                print(f"  Cumulative infected fraction: {total_cases/pop*100:.4f}%")

    except urllib.error.URLError as e:
        print(f"Error downloading data: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        os.close(tmp_fd)
        os.unlink(tmp_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--country", default="Italy",
                        help="Country name as in OWID dataset (default: Italy)")
    parser.add_argument("--start", default="2020-02-21",
                        help="Start date YYYY-MM-DD (default: 2020-02-21)")
    parser.add_argument("--end", default="2020-05-18",
                        help="End date YYYY-MM-DD (default: 2020-05-18)")
    parser.add_argument("--force", action="store_true",
                        help="Re-download even if data file exists")
    args = parser.parse_args()
    fetch_and_filter(args.country, args.start, args.end, force=args.force)


if __name__ == "__main__":
    main()
