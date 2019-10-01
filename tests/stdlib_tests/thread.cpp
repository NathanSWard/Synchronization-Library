// thread.cpp
#include "../catch.hpp"
#include "../../stdlib/thread.hpp"

#include <chrono>
#include <functional>

void func(int& i) {
    ++i;
}

struct foo {
    void bar() noexcept {++n;}
    int n{0};
};

struct callable {
    void operator()() noexcept {++n;}   
    int n{0};
};

void do_work() {
    for (volatile auto i{0}; i < 10; ++i);
}

TEST_CASE("sync::thread::thread, [thread]") {
    
    SECTION("default construction") {
        sync::thread t{};
        CHECK(!t.joinable());
    }
    
    SECTION("function construction") {
        
        SECTION("lambda") {
            bool executed{false};
            sync::thread t{[&](){executed = true;}};
            REQUIRE(t.joinable());
            t.join();
            CHECK(executed == true);
        }

        SECTION("function pointer") {
            int i{0};
            sync::thread t{&func, std::ref(i)};
            REQUIRE(t.joinable());
            t.join();
            CHECK(i == 1);
        }

        SECTION("class method") {
            foo f;
            sync::thread t{&foo::bar, &f};
            t.join();
            CHECK(f.n == 1);
        }

        SECTION("callable object") {
            callable c;
            sync::thread t{std::ref(c)};
            t.join();
            CHECK(c.n == 1);
        }
    }
}

TEST_CASE("sync::thread::get_id, [thread]") {
    SECTION("get_id") {
        sync::thread t1{[](){}};
        sync::thread t2{[](){}};
        CHECK(t1.get_id() != t2.get_id());
        t1.join();
        t2.join();
    }
}

TEST_CASE("sync::thread::swap, [thread]") {
    SECTION("swap") {
        auto do_work = []() {
            for (volatile auto i{0}; i < 10; ++i);
        };

        sync::thread t1;
        sync::thread t2{do_work};

        REQUIRE(!t1.joinable());
        REQUIRE(t2.joinable());

        t1.swap(t2);

        REQUIRE(t1.joinable());
        REQUIRE(!t2.joinable());

        t1.join();
    }
}

TEST_CASE("sync::thread::hardward_concurrency, [thread]") {
    SECTION("hardward_concurrency") {
        volatile auto con = sync::thread::hardward_concurrency();
    }
}

TEST_CASE("sync::this_thread::get_id, [thread]") {
    sync::thread t{&do_work};
    CHECK(t.get_id() != sync::this_thread::get_id());
    t.join();
}

TEST_CASE("sync::this_thread::sleep_for/sleep_until, [thread]") {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    auto start = high_resolution_clock::now();

    SECTION("sleep_for") {
        sync::this_thread::sleep_for(10ms);
    }

    SECTION("sleep_until") {
        sync::this_thread::sleep_until(start + 10ms);
    }

    auto end = high_resolution_clock::now();

    CHECK(duration_cast<milliseconds>(end - start).count() >= milliseconds{10}.count());
}

TEST_CASE("sync::this_thread::yield, [thread]") {
    sync::this_thread::yield();
}