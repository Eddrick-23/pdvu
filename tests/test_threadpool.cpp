#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "threadpool.h"
#include "parser.h"

class MockParser : public IParser{
public:
    MOCK_METHOD(void, clear_doc, (), (override));
    MOCK_METHOD(bool, load_document, (const std::string&), (override));
    MOCK_METHOD(const std::string&, get_document_name, (), (const, override));
    MOCK_METHOD(PageSpecs, page_specs, (int, float), (const, override));
    MOCK_METHOD(int, num_pages, (), (const, override));
    MOCK_METHOD(void, write_page,
                (int, int, int, float, float, unsigned char*, size_t), (override));
    MOCK_METHOD(void, write_section,
                (int, int, int, float, float, fz_display_list*, unsigned char*, fz_rect), (override));
    MOCK_METHOD(std::unique_ptr<IParser>, duplicate, (), (const, override));
};

TEST(ThreadPoolTest, CreatePool) {
    auto mock = std::make_unique<MockParser>();
    EXPECT_CALL(*mock, duplicate()).Times(2);
    auto pool = ThreadPool(*mock, 2);
}

TEST(ThreadPoolTest, EnqueueTaskSingleThread) {
    auto task = [](IParser& parser) { // takes in an IParser reference
        return parser.get_document_name();
    };
    auto mock = std::make_unique<MockParser>();
    EXPECT_CALL(*mock, duplicate()).WillRepeatedly([]() {
        auto worker_mock = std::make_unique<MockParser>();

        ON_CALL(*worker_mock, get_document_name())
            .WillByDefault(testing::ReturnRefOfCopy(std::string("Mock Doc")));
        return worker_mock;
    });
    auto pool = ThreadPool(*mock, 1);

    auto fut = pool.enqueue_with_future(task);
    EXPECT_EQ(fut.get(), "Mock Doc");
}

TEST(TheadPoolTest, EnqueueTaskSingleThreadMultipleTasks) {
    std::vector<std::string> results;
    results.reserve(10);
    for (int i = 0; i < 10; i++) {
        results.push_back(std::format("result from task:{}", i));
    }
    std::vector<std::function<std::string(IParser&)>> tasks;
    tasks.reserve(10);
    for (int i = 0; i < 10; i++) {
        auto task = [i, &results](IParser& parser) { // takes in an IParser reference
            return results.at(i);
        };
        tasks.emplace_back(task);
    }
    auto parser = std::make_unique<MockParser>();
    auto pool = ThreadPool(*parser, 1);
    // enqueue the tasks and get their results
    std::vector<std::future<std::string>> futures;
    futures.reserve(10);
    for (int i = 0; i < 10; i++) {
        auto fut = pool.enqueue_with_future(tasks.at(i));
        futures.push_back(std::move(fut)); // futures are move only
    }
    // check the results
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(futures.at(i).get(), results.at(i));
    }
}

TEST(ThreadPoolTest, EnqueueTaskMultipleThreads) {
    const int n_threads = 3;

    auto task = [](IParser& parser) { // takes in an IParser reference
        return parser.get_document_name();
    };
    auto mock = std::make_unique<MockParser>();
    EXPECT_CALL(*mock, duplicate()).WillRepeatedly([]() {
        auto worker_mock = std::make_unique<MockParser>();

        ON_CALL(*worker_mock, get_document_name())
            .WillByDefault(testing::ReturnRefOfCopy(std::string("Mock Doc")));
        return worker_mock;
    });

    auto pool = ThreadPool(*mock, n_threads);

    auto fut = pool.enqueue_with_future(task);

    EXPECT_EQ(fut.get(), "Mock Doc");
}

TEST(ThreadPoolTest, EnqueueTaskMultipleThreadsMultipleTasks) {
    const int n_threads = 3;
    const int n_tasks = 3;
    std::vector<std::string> results;
    results.reserve(n_tasks);
    for (int i = 0; i < n_tasks; i++) {
        results.push_back(std::format("result from task:{}", i));
    }
    std::vector<std::function<std::string(IParser&)>> tasks;
    tasks.reserve(n_tasks);
    for (int i = 0; i < n_tasks; i++) {
        auto task = [i, &results](IParser& parser) { // takes in an IParser reference
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return results.at(i);
        };
        tasks.emplace_back(task);
    }
    auto parser = std::make_unique<MockParser>();
    auto pool = ThreadPool(*parser, n_threads);
    // enqueue the tasks and get their results
    std::vector<std::future<std::string>> futures;
    futures.reserve(n_tasks);
    for (int i = 0; i < n_tasks; i++) {
        auto fut = pool.enqueue_with_future(tasks.at(i));
        futures.push_back(std::move(fut)); // futures are move only
    }
    // check the results
    for (int i = 0; i < n_tasks; i++) {
        EXPECT_EQ(futures.at(i).get(), results.at(i));
    }
}
