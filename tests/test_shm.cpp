#include <utils/shm.h>
#include <gtest/gtest.h>
#include <sys/fcntl.h>

// TODO test write_data

bool shm_exists(const char* name) {
    int fd = shm_open(name, O_RDONLY, 0);
    if (fd == -1) {
        if (errno == ENOENT) { // doesnt exist
            return  false;
        }
        return  false; // some other error
    }
    close(fd);
    return true;
}

TEST(ShmTest, CreationAndDeletion) {
    std::string shm_name;
    {
        auto test = SharedMemory(1000);
        shm_name = test.name();
        EXPECT_TRUE(shm_exists(test.name().c_str()));
    }
    EXPECT_FALSE(shm_exists(shm_name.c_str()));
}

TEST(ShmTest, MoveConstructorTransfersOwnership) {
    std::string shm_name;
    {
        auto source = SharedMemory(1000);
        shm_name = source.name();
        EXPECT_TRUE(shm_exists(source.name().c_str()));

        auto target = std::move(source);
        EXPECT_EQ(target.name(), shm_name);
        EXPECT_TRUE(shm_exists(target.name().c_str()));

        // Check that source is cleared
        EXPECT_TRUE(source.name().empty());
        EXPECT_EQ(source.data(), MAP_FAILED);
    }
    // check deletion
    EXPECT_FALSE(shm_exists(shm_name.c_str()));
}

TEST(ShmTest, MoveAssignmentTransfersOwnership) {
    std::string shm_name_source;
    std::string shm_name_target;
    {
        auto source = SharedMemory(1000);
        auto target = SharedMemory(1000);
        shm_name_source = source.name();
        shm_name_target = target.name();

        EXPECT_TRUE(shm_exists(source.name().c_str()));
        EXPECT_TRUE(shm_exists(target.name().c_str()));

        target = std::move(source);

        // check that target now has source data
        EXPECT_EQ(target.name(), shm_name_source);
        EXPECT_TRUE(shm_exists(target.name().c_str()));

        // check that source data is moved
        EXPECT_TRUE(source.name().empty());
        EXPECT_EQ(source.data(), MAP_FAILED);

        // check that target data is deleted
        EXPECT_FALSE(shm_exists(shm_name_target.c_str()));
    }
    // check deletion
    EXPECT_FALSE(shm_exists(shm_name_source.c_str()));
}

TEST(ShmTest, WriteData) {
    std::string data = "test data";
    std::string shm_name;
    {
        auto test_shm = SharedMemory(1000);
        shm_name = test_shm.name();
        memcpy(test_shm.data(), data.c_str(), data.size());

        auto read_string = std::string(static_cast<char*>(test_shm.data()), data.size());

        EXPECT_EQ(read_string, data);
    }
    EXPECT_FALSE(shm_exists(shm_name.c_str()));
}