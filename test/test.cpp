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
        CHECK(o1 == o3);
    }

    SECTION("copy construction from non-empty object")
    {
        object o3(o2);
        CHECK(o2);
        CHECK(o3);
        CHECK(o2.type() == typeid(tracker));
        CHECK(o3.type() == typeid(tracker));
        CHECK(tracker::count == 1);
        CHECK(o2 == o3);
    }

    SECTION("move construction from empty object")
    {
        object o3(std::move(o1));
        CHECK_FALSE(o1);
        CHECK_FALSE(o3);
        CHECK(o1.type() == typeid(void));
        CHECK(o3.type() == typeid(void));
        CHECK(o1 == o3);
    }

    SECTION("move construction from non-empty object")
    {
        object o3(std::move(o2));
        CHECK_FALSE(o2);
        CHECK(o3);
        CHECK(o2.type() == typeid(void));
        CHECK(o3.type() == typeid(tracker));
        CHECK(tracker::count == 1);
        CHECK(o1 == o2);
        CHECK(o2 != o3);
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
            CHECK(o2 != o3);
        }
        CHECK(tracker::count == 1);
    }
}

TEST_CASE("object cast")
{
    object o1;
    object o2(2);

    SECTION("cast empty object")
    {
        CHECK_THROWS_AS(object_cast<int>(o1), bad_object_cast);
        CHECK_NOTHROW(object_cast<int>(&o1));
        CHECK(object_cast<int>(&o1) == nullptr);
    }

    SECTION("cast non-empty object")
    {
        CHECK(o2.type() == typeid(int));

        CHECK_NOTHROW(object_cast<int>(o2));
        CHECK_THROWS_AS(object_cast<float>(o2), bad_object_cast);

        CHECK_NOTHROW(object_cast<int>(&o2));
        CHECK_NOTHROW(object_cast<float>(&o2));

        CHECK(object_cast<int>(o2) == 2);
        CHECK(*object_cast<int>(&o2) == 2);
        CHECK(object_cast<float>(&o2) == nullptr);
        CHECK(std::addressof(object_cast<int>(o2)) == object_cast<int>(&o2));

        CHECK(unsafe_object_cast<int>(o2) == 2);
        CHECK(*unsafe_object_cast<int>(&o2) == 2);
        CHECK(std::addressof(unsafe_object_cast<int>(o2)) == unsafe_object_cast<int>(&o2));
        CHECK(object_cast<int>(&o2) == unsafe_object_cast<int>(&o2));
    }
}

TEST_CASE("shared ownership")
{
    object o1(1);
    object o2(o1);

    SECTION("take address")
    {
        CHECK(o1 == o2);
        CHECK(object_cast<int>(&o1) == object_cast<int>(&o2));
    }

    SECTION("update object")
    {
        ++object_cast<int>(o1);
        CHECK(object_cast<int>(o1) == 2);
        CHECK(object_cast<int>(o2) == 2);
    }
}

TEST_CASE("assignment and relational operators")
{
    object o1;
    object o2(tracker{});
    object o3;

    SECTION("empty object are identity")
    {
        CHECK(o1 == o3);
        CHECK(o1 <= o3);
        CHECK(o1 >= o3);
        CHECK_FALSE(o1 != o3);
        CHECK_FALSE(o1 < o3);
        CHECK_FALSE(o1 > o3);
    }

    SECTION("self copy assignment of empty object")
    {
        o1 = o1;
        CHECK(o1 == o3);
    }

    SECTION("self move assignment of empty object")
    {
        o1 = std::move(o1);
        CHECK(o1 == o3);
    }

    SECTION("self copy assignment of non-empty object")
    {
        o2 = o2;
        CHECK(tracker::count == 1);
        CHECK(o2.type() == typeid(tracker));
    }

    SECTION("self move assignment of non-empty object")
    {
        o2 = std::move(o2);
        CHECK(tracker::count == 1);
        CHECK(o2.type() == typeid(tracker));
    }

    SECTION("assign empty object to non-empty object")
    {
        o2 = o3;
        CHECK_FALSE(o2);
        CHECK(o2 == o3);
        CHECK(o1 == o2);
        CHECK(tracker::count == 0);
    }

    SECTION("assign non-empty object to empty object")
    {
        o3 = o2;
        CHECK(o3);
        CHECK(o2 == o3);
        CHECK(o1 != o3);
        CHECK(tracker::count == 1);
    }
}
