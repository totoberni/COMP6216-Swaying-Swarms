#include "config_loader.h"
#include "components.h"
#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

class ConfigLoaderTest : public ::testing::Test {
protected:
    std::string tmp_path_;

    void SetUp() override {
        tmp_path_ = "test_config_tmp.ini";
    }

    void TearDown() override {
        std::remove(tmp_path_.c_str());
    }

    void write_file(const std::string& content) {
        std::ofstream f(tmp_path_);
        f << content;
    }
};

TEST_F(ConfigLoaderTest, MissingFileReturnsFalse) {
    SimConfig config{};
    EXPECT_FALSE(load_config("nonexistent_file_12345.ini", config));
    // Defaults should be unchanged
    EXPECT_FLOAT_EQ(config.p_infect_normal, 0.5f);
    EXPECT_EQ(config.initial_normal_count, 200);
}

TEST_F(ConfigLoaderTest, EmptyFileReturnsTrue) {
    write_file("");
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));
    // All defaults preserved
    EXPECT_FLOAT_EQ(config.p_cure, 0.8f);
}

TEST_F(ConfigLoaderTest, CommentsAndBlankLinesIgnored) {
    write_file(
        "# This is a comment\n"
        "; This is also a comment\n"
        "\n"
        "   \n"
        "# Another comment\n"
    );
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));
    EXPECT_FLOAT_EQ(config.p_cure, 0.8f);
}

TEST_F(ConfigLoaderTest, SectionHeadersIgnored) {
    write_file(
        "[some_section]\n"
        "p_cure = 0.3\n"
        "[another]\n"
        "t_death = 10.0\n"
    );
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));
    EXPECT_FLOAT_EQ(config.p_cure, 0.3f);
    EXPECT_FLOAT_EQ(config.t_death, 10.0f);
}

TEST_F(ConfigLoaderTest, ParsesAllFloatFields) {
    write_file(
        "p_initial_infect_normal = 0.1\n"
        "p_initial_infect_doctor = 0.2\n"
        "p_infect_normal = 0.3\n"
        "p_infect_doctor = 0.4\n"
        "p_offspring_normal = 0.55\n"
        "p_offspring_doctor = 0.15\n"
        "p_cure = 0.9\n"
        "p_become_doctor = 0.07\n"
        "p_antivax = 0.2\n"
        "r_interact_normal = 35.0\n"
        "r_interact_doctor = 45.0\n"
        "t_death = 7.0\n"
        "t_adult = 10.0\n"
        "offspring_mean_normal = 3.0\n"
        "offspring_stddev_normal = 0.5\n"
        "offspring_mean_doctor = 2.0\n"
        "offspring_stddev_doctor = 0.8\n"
        "world_width = 1280.0\n"
        "world_height = 720.0\n"
        "max_speed = 200.0\n"
        "max_force = 8.0\n"
        "separation_weight = 2.0\n"
        "alignment_weight = 1.5\n"
        "cohesion_weight = 0.8\n"
        "separation_radius = 30.0\n"
        "alignment_radius = 60.0\n"
        "cohesion_radius = 55.0\n"
        "reproduction_cooldown = 3.0\n"
        "debuff_p_cure_infected = 0.4\n"
        "debuff_r_interact_doctor_infected = 0.6\n"
        "debuff_p_offspring_doctor_infected = 0.3\n"
        "debuff_r_interact_normal_infected = 0.7\n"
        "debuff_p_offspring_normal_infected = 0.4\n"
        "antivax_repulsion_radius = 120.0\n"
        "antivax_repulsion_weight = 4.0\n"
    );
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));

    EXPECT_FLOAT_EQ(config.p_initial_infect_normal, 0.1f);
    EXPECT_FLOAT_EQ(config.p_initial_infect_doctor, 0.2f);
    EXPECT_FLOAT_EQ(config.p_infect_normal, 0.3f);
    EXPECT_FLOAT_EQ(config.p_infect_doctor, 0.4f);
    EXPECT_FLOAT_EQ(config.p_offspring_normal, 0.55f);
    EXPECT_FLOAT_EQ(config.p_offspring_doctor, 0.15f);
    EXPECT_FLOAT_EQ(config.p_cure, 0.9f);
    EXPECT_FLOAT_EQ(config.p_become_doctor, 0.07f);
    EXPECT_FLOAT_EQ(config.p_antivax, 0.2f);
    EXPECT_FLOAT_EQ(config.r_interact_normal, 35.0f);
    EXPECT_FLOAT_EQ(config.r_interact_doctor, 45.0f);
    EXPECT_FLOAT_EQ(config.t_death, 7.0f);
    EXPECT_FLOAT_EQ(config.t_adult, 10.0f);
    EXPECT_FLOAT_EQ(config.offspring_mean_normal, 3.0f);
    EXPECT_FLOAT_EQ(config.offspring_stddev_normal, 0.5f);
    EXPECT_FLOAT_EQ(config.offspring_mean_doctor, 2.0f);
    EXPECT_FLOAT_EQ(config.offspring_stddev_doctor, 0.8f);
    EXPECT_FLOAT_EQ(config.world_width, 1280.0f);
    EXPECT_FLOAT_EQ(config.world_height, 720.0f);
    EXPECT_FLOAT_EQ(config.max_speed, 200.0f);
    EXPECT_FLOAT_EQ(config.max_force, 8.0f);
    EXPECT_FLOAT_EQ(config.separation_weight, 2.0f);
    EXPECT_FLOAT_EQ(config.alignment_weight, 1.5f);
    EXPECT_FLOAT_EQ(config.cohesion_weight, 0.8f);
    EXPECT_FLOAT_EQ(config.separation_radius, 30.0f);
    EXPECT_FLOAT_EQ(config.alignment_radius, 60.0f);
    EXPECT_FLOAT_EQ(config.cohesion_radius, 55.0f);
    EXPECT_FLOAT_EQ(config.reproduction_cooldown, 3.0f);
    EXPECT_FLOAT_EQ(config.debuff_p_cure_infected, 0.4f);
    EXPECT_FLOAT_EQ(config.debuff_r_interact_doctor_infected, 0.6f);
    EXPECT_FLOAT_EQ(config.debuff_p_offspring_doctor_infected, 0.3f);
    EXPECT_FLOAT_EQ(config.debuff_r_interact_normal_infected, 0.7f);
    EXPECT_FLOAT_EQ(config.debuff_p_offspring_normal_infected, 0.4f);
    EXPECT_FLOAT_EQ(config.antivax_repulsion_radius, 120.0f);
    EXPECT_FLOAT_EQ(config.antivax_repulsion_weight, 4.0f);
}

TEST_F(ConfigLoaderTest, ParsesIntFields) {
    write_file(
        "initial_normal_count = 500\n"
        "initial_doctor_count = 25\n"
    );
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));
    EXPECT_EQ(config.initial_normal_count, 500);
    EXPECT_EQ(config.initial_doctor_count, 25);
}

TEST_F(ConfigLoaderTest, PartialConfigKeepsDefaults) {
    write_file("p_cure = 0.1\n");
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));
    EXPECT_FLOAT_EQ(config.p_cure, 0.1f);
    // Everything else stays default
    EXPECT_FLOAT_EQ(config.p_infect_normal, 0.5f);
    EXPECT_EQ(config.initial_normal_count, 200);
    EXPECT_FLOAT_EQ(config.max_speed, 180.0f);
}

TEST_F(ConfigLoaderTest, WhitespaceAroundKeyAndValue) {
    write_file("   p_cure   =   0.3   \n");
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));
    EXPECT_FLOAT_EQ(config.p_cure, 0.3f);
}

TEST_F(ConfigLoaderTest, UnknownKeyWarnsButContinues) {
    write_file(
        "unknown_key = 42.0\n"
        "p_cure = 0.6\n"
    );
    SimConfig config{};
    EXPECT_TRUE(load_config(tmp_path_, config));
    EXPECT_FLOAT_EQ(config.p_cure, 0.6f);
}

TEST_F(ConfigLoaderTest, MalformedLineThrows) {
    write_file("this has no equals sign\n");
    SimConfig config{};
    EXPECT_THROW(load_config(tmp_path_, config), std::runtime_error);
}

TEST_F(ConfigLoaderTest, InvalidFloatValueThrows) {
    write_file("p_cure = not_a_number\n");
    SimConfig config{};
    EXPECT_THROW(load_config(tmp_path_, config), std::runtime_error);
}

TEST_F(ConfigLoaderTest, EmptyValueThrows) {
    write_file("p_cure = \n");
    SimConfig config{};
    EXPECT_THROW(load_config(tmp_path_, config), std::runtime_error);
}
