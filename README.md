# object - ownership-shared version of std::any and std::function and more std::

`object` provides the functionalities of the following `std::` components and more:

- [std::any](#stdany)
- [std::function](#stdfunction)
- [std::function_ref](#stdfunction_ref)
- [std::shared_ptr](#stdshared_ptr)
- [std::weak_ptr](#stdweak_ptr)
- [std::enable_shared_from_this](#stdenable_shared_from_this)
- [std::array](#stdarray)
- [std::span](#stdspan)
- [std::atomic](#stdatomic)
- [std::mutex](#stdmutex)
- [std::condition_variable](#stdcondition_variable)
- [std::string](#stdstring)
- [Flexible array member](#flexible-array-member)
- [Manual management and C ABI](#manual-management-and-c-abi)


## std::any

The class `object` describes a type-safe and reference-counted container for single values of any type. Two objects are 
equivalent if and only if they are either both empty or both reference to same container. The non-member `object_cast` 
and `polymorphic_object_cast` functions provide type-safe access to the contained object.

```cpp
object a = 1;
object b = std::invalid_argument("object");

assert(object_cast<int>(a) == 1);
assert(std::strcmp(polymorphic_object_cast<std::exception>(b).what(), "object") == 0);
```

- `object` can hold array types while `std::any` decays arrays to pointers.
- `object_cast` always returns references while `std::any_cast` returns exactly templated types.
- `polymorphic_object_cast` returns references/pointers to base classes while `std::any` does not has same capability.


## std::function

`object::fn` is a specialization of `object`, that can store and invoke any [Callable](https://en.cppreference.com/w/cpp/named_req/Callable).

Invoking an empty `object::fn` results in `object_not_fn` exception being thrown.

```cpp
object::fn<int(int)> f = [](int x) { return -x; };
object::fn<int(int)> g = std::negate<>{};
assert(f(2) == -2);
assert(g(2) == -2);
assert(polymorphic_object_cast<std::negate<>>(g)(2) == -2);
```


## std::function_ref

`object::fn<(&)>` is a non-owning and non-nullable reference to a Callable.

It is [TriviallyCopyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable) and is best suitable as function parameters.

```cpp
auto invoke = [](object::fn<int(&)(int)> f, int x) { return f(x); };
assert(invoke([](int x) { return -x; }, 2) == -2);
assert(invoke(std::negate<>{}, 2) == -2);
```


## std::shared_ptr

`object::ptr<T>` and its non-nullable version `object::ref<T>` are subclasses of `object` that additionally hold a 
raw pointer to anything. The lifetime of pointee does not usually exceed the `object`, e.g., the contained object itself, 
subojects or members of the contained object. Any unrelated pointees are also allowed. This is so-called *aliasing constructor*.

```cpp
object a = std::string("object");
object::ptr<std::string> p = a;
assert(std::strcmp(p->c_str(), "object") == 0);

int b = 1;
object::ptr<int> q(p, &b);
assert(*q == 1);
```


## std::weak_ptr

`object::weak` is a smart pointer that holds a non-owning ("weak") reference to an `object`. It must be converted to 
`object` ("strong" reference) in order to access the actual object. `object::weak` becomes expired if all the strong 
references to the `object` it referenced to has ended.

```cpp
object a = 1;
object::weak w = a;
assert(object_cast<int>(w.lock()) == 1);

a = {};
assert(w.expired());
assert(!w.lock());
```

- `wait_until_expired()` is available for C++20 and later.


## std::enable_shared_from_this

Use `object::from()` and its specialized versions to obtain strong or weak references from raw pointers of objects that 
managed by `object`. Be careful that these functions are not safe as compared to `std::enable_shared_from_this`.

```cpp
struct S
{
    S()
    {
        auto sp = object::ptr<S>::from(this);
        auto wp = object::weak::from(this);
        assert(!!sp);
        assert(!!(object)sp);
        assert(!wp.expired());
        assert(wp.lock() == sp);
    }
};

object a = S{};
```

- Calling `object::from()` works from constructors while `std::enable_shared_from_this` does not work.
- Calling `object::from()` is not allowed from destructors.


## std::array

`object::vec` are dynamic size arrays and `object` can directly hold fixed size arrays.

```cpp
object o;
decltype(auto) a = o.emplace<int[4]>(1, 2, 3, 4);
object::vec<int> v = {1, 2, 3, 4};

assert(std::equal(std::begin(a), std::end(a), v.begin(), v.end()));
```


## std::span

`object::vec<&>` can refer to a contiguous sequence of objects.

```cpp
std::vector<int> v = {1, 2, 3, 4};
object::vec<int&> r = v;

int i = 2;
for (auto e : r.subspan(1, 2))
    assert(e == i++);
assert(i == 4);
```


## std::atomic

`object::atomic` can be treated as specialization of `std::atomic<object>`.

```cpp
object a = 1;
object b = 2;
object::atomic s = a;
assert(s.load() == a);
assert(s.compare_exchange_strong(a, b));
assert(s.load() == b);
```


## std::mutex

`object::atomic` satisfies the requirements of [Mutex](https://en.cppreference.com/w/cpp/named_req/Mutex).

```cpp
object::atomic mutex;
{
    assert(mutex.try_lock());
    assert(mutex.get() == object());
    mutex.set(1);
    mutex.unlock();
}
{
    std::lock_guard<object::atomic> _(mutex);
    assert(object_cast<int>(mutex.get()) == 1);
}
```

- Before C++20, `object::atomic` is a spinlock. So the critical section should be rather small.
- Since C++20, `object::atomic` takes advantage of atomic waiting operations, so it is a suitable replacement of `std::mutex`.


## std::condition_variable

`object::atomic` is also a condition varaible if C++20 atomic waiting operations are available.

```cpp
object::atomic cvm;
bool shutdown = false;

std::thread t([&] {
    std::lock_guard _(cvm);
    cvm.wait([&] { return shutdown; });
});

#define synchronized(m) if (auto _ = std::unique_lock(m))

synchronized(cvm) {
    shutdown = true;
    cvm.notify_one();
}

t.join();
```


## std::string

`object::str` uses `object::vec<CharT>` as underlying storage, and behaves much like `ATL::CStringT`.

```cpp
object::str<char> s = "hello";
static_assert(sizeof(s) == sizeof(const char*));

char buf[20];
snprintf(buf, sizeof buf, "%s, %s", s, "world");
assert(std::strcmp(buf, "hello, world") == 0);

std::string_view sv = s;
assert(sv == "hello");
```


## Flexible array member

`object::fam<T, U>` provides similarity to flexible array members of C99:

```c
struct S
{
    T t;
    U u[];
};
```

`object::fam<T, U>` guarantees that the lifetime of `T` is totally within the lifetime of `U` array. So access to `U` array 
from constructors and destructor of `T` is legal.

```cpp
struct T
{
    T()
    {
        object::vec<int&> v = object::fam<T, int>::array(this);
        assert(v.size() == 4);
    }

    ~T()
    {
        object::vec<int&> v = object::fam<T, int>::array(this);

        int i = 1;
        for (auto e : v) assert(e == i++);
    }
};

object::fam<T, int> h(4);
object::vec<int&> v = h.array();
std::iota(v.begin(), v.end(), 1);
```


## Manual management and C ABI

`object` can expose its internal representation and easily integrates into stable C ABI.

```cpp
typedef struct hstrong_* hstrong;
typedef struct hweak_* hweak;

extern "C" hstrong object_create(int val)
{
    object obj = val;
    return (hstrong)obj.release();
}

extern "C" void object_destroy(hstrong obj)
{
    (void)object((object::handle)obj);
}

extern "C" hweak object_obtain_observed(hstrong obj)
{
    return (hweak)object::weak::from((object::handle)obj).release();
}

extern "C" hstrong object_lock_observed(hweak obs)
{
    object::weak w((object::handle)obs);
    hstrong obj = (hstrong)w.lock().release();
    (void)w.release();
    return obj;
}

extern "C" void object_release_observed(hweak obs)
{
    (void)object::weak((object::handle)obs);
}
```

