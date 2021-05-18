#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "object.hpp"

#include <thread>

namespace
{
    struct tracker
    {
        static long count;
        static int seq;
        int i, j, s;

        tracker(int i = 0, int j = 0) noexcept : i(i), j(j), s(++seq) { ++count; }
        tracker(const tracker& t) noexcept : i(t.i), j(t.j), s(++seq) { ++count; }
        tracker(tracker&& t) noexcept : i(t.i), j(t.j), s(++seq) { ++count; t.i = t.j = 0; }
        ~tracker() noexcept { --count; i = j = 0; }
        virtual int id() const noexcept { return s; }
        int operator()(int k) noexcept { return i + j + k; }
    };

    long tracker::count = 0;
    int tracker::seq = 0;
}

TEST_CASE("constructor and destructor")
{
    object o1;
    object o2(tracker{});

    SECTION("default construction")
    {
        CHECK_FALSE(o1);
        CHECK(o1.type() == object::null_t());
    }

    SECTION("construction from tracker")
    {
        CHECK(o2);
        CHECK(o2.type() == object::type_id<tracker>());
    }

    SECTION("inplace construction")
    {
        int seq = tracker::seq;
        object o3(std::in_place_type<tracker>);
        CHECK(o3);
        CHECK(o3.type() == object::type_id<tracker>());
        CHECK(tracker::seq == ++seq);
    }

    SECTION("copy construction from empty object")
    {
        object o3(o1);
        CHECK_FALSE(o1);
        CHECK_FALSE(o3);
        CHECK(o1.type() == object::null_t());
        CHECK(o3.type() == object::null_t());
        CHECK(o1 == o3);
    }

    SECTION("copy construction from non-empty object")
    {
        object o3(o2);
        CHECK(o2);
        CHECK(o3);
        CHECK(o2.type() == object::type_id<tracker>());
        CHECK(o3.type() == object::type_id<tracker>());
        CHECK(tracker::count == 1);
        CHECK(o2 == o3);
    }

    SECTION("move construction from empty object")
    {
        object o3(std::move(o1));
        CHECK_FALSE(o1);
        CHECK_FALSE(o3);
        CHECK(o1.type() == object::null_t());
        CHECK(o3.type() == object::null_t());
        CHECK(o1 == o3);
    }

    SECTION("move construction from non-empty object")
    {
        object o3(std::move(o2));
        CHECK_FALSE(o2);
        CHECK(o3);
        CHECK(o2.type() == object::null_t());
        CHECK(o3.type() == object::type_id<tracker>());
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
    object o3(std::in_place_type<tracker>, 1, 2);

    SECTION("cast empty object")
    {
        CHECK_THROWS_AS(object_cast<int>(o1), bad_object_cast);
        CHECK_NOTHROW(object_cast<int>(&o1));
        CHECK(object_cast<int>(&o1) == nullptr);
    }

    SECTION("cast non-empty object")
    {
        CHECK(o2.type() == object::type_id<int>());

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

        CHECK(o3.type() == object::type_id<tracker>());
        CHECK_NOTHROW(object_cast<tracker>(o3));
        tracker& t = object_cast<tracker>(o3);
        CHECK(t.i == 1);
        CHECK(t.j == 2);
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
        CHECK(o2.type() == object::type_id<tracker>());
    }

    SECTION("self move assignment of non-empty object")
    {
        o2 = std::move(o2);
        CHECK(tracker::count == 1);
        CHECK(o2.type() == object::type_id<tracker>());
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

TEST_CASE("emplace")
{
    object o;

    o = tracker{};
    CHECK(o.type() == object::type_id<tracker>());
    CHECK(tracker::count == 1);

    o = {};
    CHECK_FALSE(o);
    CHECK(tracker::count == 0);

    int seq = tracker::seq;
    tracker& t = o.emplace<tracker>(1, 2);
    CHECK(std::addressof(t) == object_cast<tracker>(&o));
    CHECK(tracker::count == 1);
    CHECK(tracker::seq == ++seq);
    CHECK(t.i == 1);
    CHECK(t.j == 2);

    int& i = o.emplace<int>();
    CHECK(i == 0);
    CHECK(tracker::count == 0);
}

TEST_CASE("hold array")
{
    object o;
    tracker ts[2]{{1, 2}, {3, 4}};

    SECTION("emplace array with less elements")
    {
        decltype(auto) a = o.emplace<tracker[2]>(ts[0]);
        static_assert(std::is_same_v<tracker(&)[2], decltype(a)>);

        CHECK(tracker::count == 4);
        CHECK(a[0].i == 1);
        CHECK(a[0].j == 2);
        CHECK(a[1].i == 0);
        CHECK(a[1].j == 0);

        o = {};
        CHECK(tracker::count == 2);
    }

    SECTION("copy array")
    {
        o = ts;

        CHECK(tracker::count == 4);
        CHECK(o.type() == object::type_id<tracker[2]>());

        auto& a = object_cast<tracker[2]>(o);

        CHECK(a[0].i == 1);
        CHECK(a[0].j == 2);
        CHECK(a[1].i == 3);
        CHECK(a[1].j == 4);

        CHECK(a[0].i == ts[0].i);
        CHECK(a[0].j == ts[0].j);
        CHECK(a[1].i == ts[1].i);
        CHECK(a[1].j == ts[1].j);
    }

    SECTION("move array")
    {
        o = std::move(ts);

        CHECK(tracker::count == 4);
        CHECK(o.type() == object::type_id<tracker[2]>());

        auto& a = object_cast<tracker[2]>(o);

        CHECK(a[0].i == 1);
        CHECK(a[0].j == 2);
        CHECK(a[1].i == 3);
        CHECK(a[1].j == 4);

        CHECK(ts[0].i == 0);
        CHECK(ts[0].j == 0);
        CHECK(ts[1].i == 0);
        CHECK(ts[1].j == 0);
    }
}

TEST_CASE("polymorphic cast")
{
    struct derived : tracker
    {
        tracker _;
        using tracker::tracker;
        int id() const noexcept final { return j; }
    };

    object o(derived{11, 22});

    CHECK(tracker::count == 2);
    CHECK_THROWS_AS(object_cast<tracker>(o), bad_object_cast);
    CHECK(polymorphic_object_cast<void>(&o) != nullptr);
    CHECK_NOTHROW(polymorphic_object_cast<tracker>(o));

    decltype(auto) t = polymorphic_object_cast<tracker>(o);
    CHECK(t.id() == 22);

    o = {};
    CHECK(tracker::count == 0);
}

TEST_CASE("variable length array")
{
    tracker::seq = 0;
    const int n = 3;

    object o;
    decltype(auto) a = o.emplace<tracker[]>(n);
    static_assert(std::is_same_v<decltype(a), tracker(&)[]>);
    CHECK(tracker::count == n);

    for(int i = 0; i < n; ++i)
        CHECK(a[i].id() == i + 1);

    decltype(auto) b = object_cast<tracker[]>(o);
    static_assert(std::is_same_v<decltype(b), tracker(&)[]>);
    for(int i = 0; i < n; ++i)
        CHECK(b[i].id() == i + 1);

    object::vec<tracker> vv = o;
    CHECK(vv.size() == n);
    {
        int i = 0;
        for(auto& t : vv)
        {
            CHECK(t.id() == ++i);
            t.s = 0;
        }
    }
    for (std::size_t i = 0; i < vv.size(); ++i)
        CHECK(vv[i].id() == 0);

    object::vec<tracker&> rv = vv;
    CHECK(tracker::count == n);

    vv = {};
    CHECK(vv.empty());
    CHECK(rv.size() == n);
    for(const auto& t : rv)
        CHECK(t.id() == 0);

    o = {};
    CHECK(tracker::count == 0);
}

TEST_CASE("fn")
{
    auto lambda = [seed = 100](int d) mutable -> int { return d + (seed++); };
    object::fn<int(int)> f = lambda;
    object::fn<int(&)(int)> g = f;

    CHECK(f(1) == 101);
    CHECK(f(1) == 102);
    CHECK(g(1) == 103);
    CHECK(g(1) == 104);
    CHECK(g.object() == f);
    CHECK_NOTHROW(polymorphic_object_cast<decltype(lambda)>(f));

    struct S
    {
        static int echo(int e) { return e; }
    };

    f = S::echo;

    CHECK(f(1) == 1);
    CHECK(f(1) == 1);
    CHECK(g(1) == 1);
    CHECK(g(1) == 1);
    CHECK(g.object() == f);
    CHECK_NOTHROW(polymorphic_object_cast<int(int)>(f));

    int seq = tracker::seq;
    tracker& t = f.emplace<tracker>(1, 2);

    CHECK(tracker::seq == ++seq);
    CHECK(f(1) == 4);
    CHECK(g(1) == 4);

    ++t.i;

    CHECK(f(1) == 5);
    CHECK(g(1) == 5);
    CHECK(g.object() == f);
    CHECK_NOTHROW(polymorphic_object_cast<tracker>(f));

    f = std::in_place_type<tracker>;
    CHECK(tracker::seq == ++seq);
    CHECK(f(1) == 1);
    CHECK(g(1) == 1);
    CHECK(g.object() == f);
    CHECK_NOTHROW(polymorphic_object_cast<tracker>(f));

    polymorphic_object_cast<tracker>(f).i = 1;
    CHECK(f(1) == 2);
    CHECK(g(1) == 2);


    g = lambda;
    CHECK(g(1) == 101);
    CHECK(g(1) == 102);

    g = S::echo;
    CHECK(g(1) == 1);

    auto h = [](object::fn<int(&)(int)> g) { return g(1); };
    CHECK(h(tracker{1, 2}) == 4);
}

TEST_CASE("atomic")
{
    tracker::seq = 0;
    object::atomic atomic(std::in_place_type<tracker>);
    CHECK(tracker::count == 1);

    atomic.lock();
    CHECK(!atomic.try_lock());

    std::thread t([&] {
        object n(std::in_place_type<tracker>);

        atomic.set(std::in_place_type<tracker>);
        atomic.unlock();

        object o;
        while (!atomic.compare_exchange_weak(o, n)) o = {};
    });

    atomic.lock();
    atomic.unlock();

    CHECK(atomic.try_lock());
    atomic.unlock();

    CHECK(object_cast<tracker>(atomic.load()).id() == 3);
    CHECK(tracker::count == 2);

    atomic.store({});

    t.join();

    CHECK(object_cast<tracker>(atomic.exchange(std::in_place_type<tracker>)).id() == 2);
    CHECK(tracker::count == 1);

    CHECK(object_cast<tracker>(atomic).id() == 4);
}
