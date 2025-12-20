#include <filesystem>
#include <gtest/gtest.h>
#include <tempfile.h>
#include <__filesystem/filesystem_error.h>

TEST(TempfileTest, CreationAndDeletion) {
    std::string tempfile_path;
    {
        // create object in this block, it goes out of scope and is destroyed after this block
        auto test = Tempfile(1000);
        tempfile_path = test.path();

        // Check that file exists
        EXPECT_TRUE(std::filesystem::exists(tempfile_path));
    }
    EXPECT_FALSE(std::filesystem::exists(tempfile_path));
}

TEST(TempfileTest, MoveConstructorTransfersOwnership) {
    std::string tempfile_path;
    {
        auto test_source = Tempfile(1000);
        EXPECT_TRUE(std::filesystem::exists(test_source.path()));
        tempfile_path = test_source.path();
        auto test_target = std::move(test_source);
        // check that target has original data
        EXPECT_EQ(test_target.path(), tempfile_path);
        EXPECT_TRUE(std::filesystem::exists(test_target.path()));

        // check that source no longer has the original data
        EXPECT_TRUE(test_source.path().empty());
        EXPECT_EQ(test_source.data(), nullptr);
    }
    // check that destructor still runs
    EXPECT_FALSE(std::filesystem::exists(tempfile_path));
}

TEST(TempfileTest, MoveAssignmentTransfersOwnership) {
    std::string tempfile_path_source;
    std::string tempfile_path_target;
    {
        auto temp_source = Tempfile(1000);
        auto temp_target = Tempfile(1000);
        tempfile_path_source = temp_source.path();
        tempfile_path_target = temp_target.path();
        EXPECT_TRUE(std::filesystem::exists(temp_source.path()));
        EXPECT_TRUE(std::filesystem::exists(temp_target.path()));
        temp_target = std::move(temp_source);
        // check that target has source data
        EXPECT_EQ(temp_target.path(), tempfile_path_source);
        EXPECT_TRUE(std::filesystem::exists(temp_target.path()));

        // check that source no longer has the original data
        EXPECT_TRUE(temp_source.path().empty());
        EXPECT_EQ(temp_source.data(), nullptr);

        // check that target data deleted
        EXPECT_FALSE(std::filesystem::exists(tempfile_path_target));
    }
    // check that destructor still runs
    EXPECT_FALSE(std::filesystem::exists(tempfile_path_source));
}

TEST(TempfileTest, WriteData) {
    std::string data = "test data";
    std::string tempfile_path;
    {
        auto test = Tempfile(1000);
        tempfile_path = test.path();
        memcpy(test.data(), data.c_str(), data.size());

        const char* read_data= static_cast<char*>(test.data());
        // don't use strlen here since it stops at first null byte
        auto read_string = std::string(read_data, data.size());
        EXPECT_EQ(read_string, data);
    }
    EXPECT_FALSE(std::filesystem::exists(tempfile_path));
}


