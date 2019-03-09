#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "object.hpp"

namespace
{
    struct tracker
    {
        static long count;

        tracker() noexcept { ++count; }
        tracker(const tracker&) noexcept { ++count; }
        tracker(tracker&&) noexcept { ++count; }
        ~tracker() noexcept { --count; }
    };

    long tracker::count = 0;
}

TEST_CASE("constructor and destructor")
{
    object o1;
    object o2(tracker{});

    SECTION("default construction")
    {
        CHECK_FALSE(o1);
        CHECK(o1.type() == typeid(void));
    }

    SECTION("construction from tracker")
    {
        CHECK(o2);
        CHECK(o2.type() == typeid(tracker));
    }

    SECTION("copy construction from empty object")
    {
        object o3(o1);
        CHECK_FALSE(o1);
        CHECK_FALSE(o3);
        CHECK(o1.type() == typeid(void));
        CHECK(o3.type() == typeid(void));
    }

    SECTION("copy construction from non-empty object")
    {
        object o3(o2);
        CHECK(o2);
        CHECK(o3);
        CHECK(o2.type() == typeid(tracker));
        CHECK(o3.type() == typeid(tracker));
        CHECK(tracker::count == 1);
    }

    SECTION("move construction from empty object")
    {
        object o3(std::move(o1));
        CHECK_FALSE(o1);
        CHECK_FALSE(o3);
        CHECK(o1.type() == typeid(void));
        CHECK(o3.type() == typeid(void));
    }

    SECTION("move construction from non-empty object")
    {
        object o3(std::move(o2));
        CHECK_FALSE(o2);
        CHECK(o3);
        CHECK(o2.type() == typeid(void));
        CHECK(o3.type() == typeid(tracker));
        CHECK(tracker::count == 1);
    }

    SECTION("destruction")
    {
        CHECK(tracker::count == 1);
        {
            object o3(o2);
            CHECK(tracker::count == 1);
        }
        CHECK(tracker::count == 1);
        {
            object o3(tracker{});
            CHECK(tracker::count == 2);
        }
        CHECK(tracker::count == 1);
    }
}
