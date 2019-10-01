// mutex.cpp

#include "../catch.hpp"
#include "../../stdlib/mutex.hpp"
#include "../../stdlib/thread.hpp"

#include <chrono>

template<class M>
void test_mutex() {
    M m;
    m.lock();
    sync::thread t1([&]{
        CHECK(!m.try_lock());
    });
    t1.join();
    m.unlock();
    sync::thread t2([&] {
        REQUIRE(m.try_lock());
        m.unlock();
    });
    t2.join();
}

template<class M>
void test_recursive_mutex() {
    M m;

    m.lock();
    m.lock();
    REQUIRE(m.try_lock());
    m.unlock();
    m.unlock();
    m.unlock();
}

template<class M>
void test_timed_mutex() {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    M m;

    m.lock();
  
    SECTION("try_lock_for") {
        std::thread t{[&] {
            CHECK(!m.try_lock());
            auto const start = high_resolution_clock::now();
            CHECK(!m.try_lock_for(10ms));
            auto const end = high_resolution_clock::now();
            CHECK(duration_cast<milliseconds>(end - start).count() >= milliseconds{10}.count());
        }};
        t.join();
    }

    SECTION("try_lock_until") {
        std::thread t{[&] {
            CHECK(!m.try_lock());
            auto const start = high_resolution_clock::now();
            CHECK(!m.try_lock_until(start + 10ms));
            auto const end = high_resolution_clock::now();
            CHECK(duration_cast<milliseconds>(end - start).count() >= milliseconds{10}.count());
        }};
        t.join();
    }

    m.unlock();
}

TEST_CASE("sync::mutex", "[mutex]") {
    test_mutex<sync::mutex>();
}

TEST_CASE("sync::recursive_mutex", "[mutex]") {
    test_mutex<sync::recursive_mutex>();
    test_recursive_mutex<sync::recursive_mutex>();
}

TEST_CASE("sync::timed_mutex", "[mutex]") {
    test_mutex<sync::timed_mutex>();
    test_timed_mutex<sync::timed_mutex>();
}

TEST_CASE("sync::recursive_timed_mutex", "[mutex]") {
    test_mutex<sync::recursive_timed_mutex>();
    test_recursive_mutex<sync::recursive_timed_mutex>();
    test_timed_mutex<sync::recursive_timed_mutex>();
}

TEST_CASE("sync::lock", "[mutex]") {

}

TEST_CASE("sync::try_lock", "[mutex]") {

}

TEST_CASE("sync::call_once", "[mutex]") {
    
}

TEST_CASE("sync::lock_gurad", "[mutex]") {

}

TEST_CASE("sync::unique_lock", "[mutex]") {

}

TEST_CASE("sync::scoped_lock", "[mutex]") {

}