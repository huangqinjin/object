#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "object.hpp"

#include <thread>
#include <future>
#if defined(__cpp_lib_span)
#include <span>
#endif

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

    CHECK(atomic.try_lock());
    atomic.unlock();

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

    CHECK(object_cast<tracker>(atomic.load()).id() == 3);
    CHECK(tracker::count == 2);

    atomic.store({});

    t.join();

    CHECK(object_cast<tracker>(atomic.exchange(std::in_place_type<tracker>)).id() == 2);
    CHECK(tracker::count == 1);

    object obj = atomic;
    CHECK(obj.type() == object::type_id<tracker>());
    CHECK(object_cast<tracker>(obj).id() == 4);
}

#if defined(__cpp_lib_atomic_wait)
TEST_CASE("condition variable")
{
    object::atomic cvm;
    bool shutdown = false;

    std::promise<void> p;
    auto f = p.get_future();
    std::thread t([&] {
        std::lock_guard _(cvm);
        CHECK(shutdown == false);
        p.set_value();
        cvm.wait([&] { return shutdown; });
        CHECK(shutdown == true);
        shutdown = false;
    });
    f.wait();

#define synchronized(m) if (auto _ = std::unique_lock(m))

    synchronized(cvm) {
        shutdown = true;
        cvm.notify_one();
        CHECK(shutdown == true);
    }

    t.join();
    CHECK(shutdown == false);
}
#endif

TEST_CASE("from")
{
    struct resource : tracker
    {
        object::ptr<resource> shared_from_this() const
        {
            return object::ptr<resource>::from(this);
        }
    };

    {
        object o;
        auto& r = o.emplace<resource>();
        CHECK(tracker::count == 1);

        auto p = r.shared_from_this();
        CHECK(p == o);
    }

    CHECK(tracker::count == 0);
}

TEST_CASE("fam")
{
    struct tracker2 : tracker
    {
        tracker2() : tracker(13, 14)
        {
            CHECK(s == count);
        }

        ~tracker2()
        {
            CHECK(s == count);
        }
    };

    struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__ * 2) resource : tracker2
    {
        void check()
        {
            CHECK((std::uintptr_t)this % alignof(resource) == 0);

            int id = 0;
            for (auto& t : trackers())
            {
                CHECK(t.i == 13);
                CHECK(t.j == 14);
                CHECK(t.s == ++id);
            }

            CHECK(i == 13);
            CHECK(j == 14);
            CHECK(s == ++id);
            CHECK(tracker::count == s);
        }

        resource()
        {
            check();
        }

        ~resource()
        {
            check();
        }

        object::vec<tracker2&> trackers()
        {
            return object::fam<resource, tracker2>::array(this);
        }

        static object::ptr<resource> create(std::size_t n)
        {
            object::fam<resource, tracker2> obj;
            obj.emplace(n);
            return obj;
        }
    };

    tracker::seq = 0;
    tracker::count = 0;

    auto p = resource::create(3);
    CHECK(tracker::seq == 4);
    CHECK(tracker::count == 4);

    p = {};
    CHECK(tracker::seq == 4);
    CHECK(tracker::count == 0);
}

TEST_CASE("str")
{
    SECTION("std::string")
    {
        object::ls s = std::string("1111");
        CHECK(s.size() == 4);
        CHECK(std::distance(s.begin(), s.end()) == s.size());
        for (auto c : s) CHECK(c == '1');

        CHECK(object(s).type() == object::type_id<char[]>());
    }

    SECTION("std::string_view")
    {
        object::ws s = std::wstring_view(L"1111");
        CHECK(s.size() == 4);
        CHECK(std::distance(s.begin(), s.end()) == s.size());
        for (auto c : s) CHECK(c == L'1');

        CHECK(object(s).type() == object::type_id<wchar_t[]>());
    }

    SECTION("C string")
    {
        object::u16s s = u"1111";
        CHECK(s.size() == 4);
        CHECK(std::distance(s.begin(), s.end()) == s.size());
        for (auto c : s) CHECK(c == u'1');

        CHECK(object(s).type() == object::type_id<char16_t[]>());
    }
}

TEST_CASE("vec destruct order and alignment")
{
    struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__ * 2) tracker2 : tracker
    {
        tracker2()
        {
            CHECK((std::uintptr_t)this % alignof(tracker2) == 0);
            CHECK(s == count);
        }

        ~tracker2()
        {
            CHECK(s == count);
        }
    };

    tracker::seq = 0;
    tracker::count = 0;

    object::vec<tracker2> v(3);
    CHECK(tracker::seq == 3);
    CHECK(tracker::count == 3);

    v = {};
    CHECK(tracker::seq == 3);
    CHECK(tracker::count == 0);
}

TEST_CASE("aliasing constructor")
{
    struct tracker2 : tracker {};

    object obj = tracker2();

    CHECK_NOTHROW((object::ptr<tracker2>(obj)));
    CHECK_NOTHROW((object::ptr<tracker2>(obj, nullptr)));
    CHECK_NOTHROW((object::ptr<tracker>(obj, nullptr)));
    CHECK_THROWS_AS((object::ptr<tracker>(obj)), bad_object_cast);

    CHECK_NOTHROW((object::ref<tracker2>(obj)));
    CHECK_NOTHROW((object::ref<tracker2>(obj, nullptr)));
    CHECK_NOTHROW((object::ref<tracker>(obj, nullptr)));
    CHECK_THROWS_AS((object::ref<tracker>(obj)), bad_object_cast);
}

TEST_CASE("weak")
{
    tracker::count = 0;
    object obj = tracker();
    object::weak wp = obj;
    CHECK(tracker::count == 1);

    CHECK(!!obj);
    CHECK(wp.expired() == false);
    CHECK(wp.lock() == obj);
    CHECK_NOTHROW((object)wp);

    obj = {};
    CHECK(tracker::count == 0);

    CHECK(wp.expired() == true);
    CHECK(!wp.lock());
    CHECK_THROWS_AS((object)wp, bad_weak_object);
}

#if defined(__cpp_lib_atomic_wait)
TEST_CASE("weak wait")
{
    object obj = 1;
    object::weak wp = obj;

    std::promise<void> p;
    auto f = p.get_future();
    std::thread t([&]{
        CHECK(wp.expired() == false);
        p.set_value();
        wp.wait_until_expired();
        CHECK(wp.expired() == true);
    });
    f.wait();

    obj = {};

    t.join();
    CHECK(wp.expired() == true);
}
#endif

TEST_CASE("alignment")
{
    struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__ * 8) A
    {
        A() noexcept
        {
            CHECK((std::uintptr_t)this % alignof(A) == 0);
        }
    };

    struct placeholder
    {
        std::atomic<unsigned> strong_ref_counter;
        std::atomic<unsigned> weak_ref_counter;
        void* vptr;
    };

    SECTION("vec")
    {
        struct holder : placeholder
        {
            std::ptrdiff_t array_length;
        };

        object::vec<A> v(2);

        auto p = *reinterpret_cast<const unsigned char**>(std::addressof(v));
        auto a = reinterpret_cast<const unsigned char*>(v.data());

        CHECK((std::uintptr_t)p % alignof(A) == 0);
        CHECK(p + (sizeof(holder) + alignof(A) - 1) / alignof(A) * alignof(A) == a);
    }

    SECTION("fam")
    {
        using T = char;
        object::fam<T, A> v(2);

        struct holder : placeholder
        {
            T obj;
            std::ptrdiff_t array_length;
        };

        auto p = *reinterpret_cast<const unsigned char**>(std::addressof(v));
        auto a = reinterpret_cast<const unsigned char*>(v.array().data());

        CHECK((std::uintptr_t)p % alignof(A) == 0);
        CHECK(p + (sizeof(holder) + alignof(A) - 1) / alignof(A) * alignof(A) == a);
    }
}

#if defined(__cpp_lib_ranges)
TEST_CASE("std::ranges with vec")
{
    using T = int;

    static_assert(std::ranges::view<object::vec<T>>);
    static_assert(std::ranges::view<object::vec<T&>>);
    static_assert(std::ranges::borrowed_range<object::vec<T&>>);

    int a[] = { 1, 2, 3, 4 };
    object::vec<int> v(a, 4);

    CHECK(std::ranges::equal(v | std::views::reverse, a | std::views::reverse));
    CHECK(std::ranges::equal(object::vec<int&>(a) | std::views::drop(1), std::views::counted(a + 1, 3)));

#if defined(__cpp_lib_span)
    auto bytes = as_bytes(std::span(object::vec<int&>(a)));
    CHECK(bytes.size() == sizeof(a));
    CHECK(bytes.data() == (const void*)&a);
#endif
}

TEST_CASE("std::ranges with str")
{
    static_assert(std::ranges::view<object::str<char>>);

    object::str<char> s = "1 2 3 4";
    int a[] = { 1, 2, 3, 4 };

#if __cpp_lib_ranges >= 202110L
    auto ints = s
        | std::views::split(' ')
        | std::views::transform([](auto r) { return std::string(r.data(), r.size()); })
        | std::views::transform([](auto s) { return std::stoi(s); });

    CHECK(std::ranges::equal(a, ints));
#endif
}
#endif
