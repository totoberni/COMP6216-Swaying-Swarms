#pragma once

#include <cstdio>
#include <string>

struct SimStats;

// Directory management
std::string create_output_dir(const char* base);

// CSV time-series export (incremental)
FILE* open_csv(const std::string& dir);
void write_csv_row(FILE* f, int frame, float time_s, const SimStats& stats);
void close_csv(FILE* f);

// Config snapshot
void snapshot_config(const std::string& dir, const std::string& config_path);

// Final summary
void export_summary(const std::string& dir, const SimStats& stats,
                    float duration, int peak_infected, float peak_pct);
