//
// Copyright (c) 2019-2022 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <type_traits>
#include <cassert>
#include <atomic>
#include <cstdint>        // uintptr_t
#include <utility>        // move, forward, exchange, in_place_type_t, integer_sequence
#include <memory>         // addressof, uninitialized_value_construct_n, uninitialized_default_construct_n
#include <new>            // operator new, operator delete, align_val_t, launder
#include <stdexcept>      // out_of_range
#include <algorithm>      // copy_n, fill_n, max
#include <string>
#include <string_view>

class bad_object_cast : public std::exception {};
class object_not_fn : public bad_object_cast {};
class bad_weak_object : public std::exception {};

class object__weak;

class object__atomic;

template<typename F>
class object__fn;

template<typename T>
class object__ptr;

template<typename T>
class object__ref;

template<typename T>
class object__vec;

template<typename T, typename U>
class object__fam;

template<typename CharT>
class object__str;

class object
{
    template<typename T>
    static void t() noexcept {}

public:
    using type_index = void (*)() noexcept;

    template<typename T>
    [[nodiscard]] static type_index type_id() noexcept
    {
        return &t<std::remove_cv_t<T>>;
    }

    [[nodiscard]] static type_index null_t() noexcept
    {
        return type_id<void>();
    }

    template<typename T>
    static void destroy_at(T* p) noexcept(std::is_nothrow_destructible_v<T>)
    {
        // C++20 destroy_at calls the destructors of array in order.
        // But raw array destructor is in reverse order.
        if constexpr (std::is_trivially_destructible_v<T>)
            return;
        else if constexpr (std::is_array_v<T>)
            for (std::size_t i = std::extent_v<T>; i > 0; --i)
                destroy_at(std::addressof((*p)[i - 1]));
        else
            p->~T();
    }

protected:
    template<typename ...Args>
    struct is_args_one : std::false_type { using type = void; };

    template<typename Arg>
    struct is_args_one<Arg> : std::true_type { using type = Arg; };

    template<typename T>
    struct is_in_place_type : std::false_type { using type = T; };

    template<typename T>
    struct is_in_place_type<std::in_place_type_t<T>> : std::true_type { using type = T; };

    template<typename T>
    using rmcvr = std::remove_cv_t<std::remove_reference_t<T>>;

    template<typename T, typename U = rmcvr<T>>
    using enable = std::enable_if_t<!std::is_convertible_v<U, object> && !is_in_place_type<U>::value, U>;

    template<typename T>
    class held
    {
        union { T t; };

        template<std::size_t I, std::size_t N, typename U>
        static U& get(U(&a)[N]) { return a[I]; }

        template<std::size_t I, std::size_t N, typename U>
        static U&& get(U(&&a)[N]) { return static_cast<U&&>(a[I]); }

        template<typename... Args>
        void init(std::true_type, Args&&... args)
        {
            (void) new((void*)std::addressof(t)) T(std::forward<Args>(args)...);
        }

        template<typename... Args>
        void init(std::false_type, Args&&... args)
        {
            (void) new((void*)std::addressof(t)) T{std::forward<Args>(args)...};
        }

        template<typename A, std::size_t... I>
        void init(std::index_sequence<I...>, A&& a)
        {
            (void) new((void*)std::addressof(t)) T{get<I>(static_cast<A&&>(a))...};
        }

        template<typename... Args, typename U = rmcvr<typename is_args_one<Args...>::type>,
                 typename = std::enable_if_t<std::is_array_v<T> && std::is_same_v<T, U>>>
        void init(int&&, Args&&... args)
        {
            return init(std::make_index_sequence<std::extent_v<U>>{}, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void init(const int&, Args&&... args)
        {
            return init(std::is_constructible<T, Args&&...>{}, std::forward<Args>(args)...);
        }

    public:
        T& value() noexcept { return t; }

        template<typename... Args>
        void construct(Args&&... args)
        {
            init(0, std::forward<Args>(args)...);
        }

        void destruct()
        {
            object::destroy_at(std::addressof(value()));
        }

        held() noexcept {}
        ~held() noexcept {}

        template<typename... Args>
        explicit held(bool init, Args&&... args)
        {
            if (init) construct(std::forward<Args>(args)...);
        }
    };

    template<typename T>
    class held<T[]>
    {
        const std::ptrdiff_t n;

    public:
        static constexpr std::size_t alignment = (std::max)(alignof(std::ptrdiff_t), alignof(T));

        explicit held(std::ptrdiff_t n, void* this1) : n(std::abs(n))
        {
            if (n > 0) std::uninitialized_value_construct_n(value(this1), this->n);
            else std::uninitialized_default_construct_n(value(this1), this->n);
        }

        void destruct(void* this1)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (T * const q = value(this1), *p = q + n; p != q; )
                    object::destroy_at(std::addressof(*--p));
        }

        void* operator new(std::size_t sz, std::ptrdiff_t n)
        {
            return ::operator new(sz + n * sizeof(T));
        }

        void* operator new(std::size_t sz, std::align_val_t al, std::ptrdiff_t n)
        {
            return ::operator new(sz + n * sizeof(T), al);
        }

        // called in create() if the constructor throws an exception, could be private
        void operator delete(void* ptr, std::ptrdiff_t)
        {
            return ::operator delete(ptr);
        }

        // called in create() if the constructor throws an exception, could be private
        void operator delete(void* ptr, std::align_val_t al, std::ptrdiff_t)
        {
            return ::operator delete(ptr, al);
        }

        // called in destructor of object when delete, must be public
        void operator delete(void* ptr)
        {
            return ::operator delete(ptr);
        }

        // called in destructor of object when delete, must be public
        void operator delete(void* ptr, std::align_val_t al)
        {
            return ::operator delete(ptr, al);
        }

        auto value(void* this1 /*= this + 1*/) noexcept -> T(&)[]
        {
            return */*std::launder*/(static_cast<T(*)[]>(this1));
        }

        std::ptrdiff_t length() const noexcept
        {
            return n;
        }
    };

    class refcounted
    {
    public:
        using counter_t = unsigned int;
        std::atomic<counter_t> refcount = 1;

        counter_t xref() noexcept
        {
            counter_t r = refcount.load(std::memory_order_relaxed);
            while (r != 0 && !refcount.compare_exchange_weak(r, r + 1, std::memory_order_relaxed));
            return r == 0 ? 0 : r + 1;
        }

        counter_t count() const noexcept { return refcount.load(std::memory_order_relaxed); }
        counter_t addref() noexcept { return refcount.fetch_add(1, std::memory_order_relaxed) + 1; }
        counter_t release() noexcept { return refcount.fetch_sub(1, std::memory_order_relaxed) - 1; }
    };

    class weakable
    {
    public:
        refcounted weak;
    };

    class placeholder : public refcounted, public weakable
    {
    public:
        virtual ~placeholder() = default;
        [[nodiscard]] virtual type_index type() const noexcept = 0;
        [[noreturn]] virtual void throws() { throw nullptr; }

        virtual void destroy() noexcept /* = 0 */
        {
#if defined(__cpp_lib_atomic_wait)
            refcount.notify_all();
#endif
            if (weak.release() != 0) return;
            delete this;
        }
    } *p;

    template<typename T, typename ...>
    class holder : public placeholder, public held<T>
    {
        [[nodiscard]] type_index type() const noexcept final { return type_id<T>(); }

        [[noreturn]] void throws() final { throw std::addressof(this->value()); }

        void destroy() noexcept override
        {
            destruct();
            placeholder::destroy();
        }

    protected:
        template<typename... Args>
        void construct(Args&&... args)
        {
            held<T>::construct(std::forward<Args>(args)...);
        }

        void destruct()
        {
            held<T>::destruct();
        }

        holder() = default;

        template<typename... Args>
        explicit holder(bool, Args&&... args) : held<T>(true, std::forward<Args>(args)...) {}

    public:
        template<typename... Args>
        [[nodiscard]] static auto create(Args&&... args)
        {
            return new holder(true, std::forward<Args>(args)...);
        }
    };

    template<typename T>
    class alignas((std::max)(held<T[]>::alignment, alignof(placeholder))) holder<T[]> final
        : public placeholder, public held<T[]>
    {
        [[nodiscard]] type_index type() const noexcept final { return type_id<T[]>(); }

        void destroy() noexcept override
        {
            held<T[]>::destruct(this + 1);
            placeholder::destroy();
        }

        explicit holder(std::ptrdiff_t n) : held<T[]>(n, this + 1) {}

    public:
        decltype(auto) value() noexcept { return held<T[]>::value(this + 1); }

        [[nodiscard]] static auto create(std::ptrdiff_t n)
        {
            return new(std::abs(n)) holder(n);
        }
    };

    template<typename R, typename... Args>
    class holder<R(Args...)> : public placeholder
    {
        [[nodiscard]] type_index type() const noexcept override { return type_id<R(Args...)>(); }

    public:
        virtual R call(Args... args) = 0;

        template<typename T, typename... A, typename D = std::decay_t<T>,
                 typename F = typename is_in_place_type<D>::type,
                 typename = std::enable_if_t<std::is_invocable_r_v<R, F, Args...>>>
        [[nodiscard]] static auto create(T&& t, A&&... a)
        {
            return new holder<F, R(Args...)>(is_in_place_type<D>{}, std::forward<T>(t), std::forward<A>(a)...);
        }
    };

    template<typename F, typename R, typename... Args>
    class holder<F, R(Args...)> : public holder<R(Args...)>, public held<F>
    {
        R call(Args... args) override
        {
            return static_cast<R>(value()(std::forward<Args>(args)...));
        }

        [[noreturn]] void throws() override { throw std::addressof(value()); }

        void destroy() noexcept override
        {
            held<F>::destruct();
            placeholder::destroy();
        }

    public:
        std::remove_pointer_t<F>& value() noexcept
        {
            if constexpr (std::is_pointer_v<F>) return *held<F>::value();
            else return held<F>::value();
        }

        template<typename... A>
        explicit holder(std::false_type, A&&... a) : held<F>(true, std::forward<A>(a)...) {}

        template<typename T, typename... A>
        explicit holder(std::true_type, T&&, A&&... a) : holder(std::false_type{}, std::forward<A>(a)...) {}
    };

    template<typename T, typename U>
    class alignas((std::max)(held<U[]>::alignment, alignof(holder<T>))) holder<T, U[]> final
        : public holder<T>, public held<U[]>
    {
        void destroy() noexcept override
        {
            holder<T>::destruct();
            held<U[]>::destruct(this + 1);
            placeholder::destroy();
        }

        template<typename... Args>
        explicit holder(std::ptrdiff_t n, Args&&... args) : held<U[]>(n, this + 1)
        {
            holder<T>::construct(std::forward<Args>(args)...);
        }

    public:
        using holder<T>::value;

        decltype(auto) array() noexcept { return held<U[]>::value(this + 1); }

        template<typename... Args>
        [[nodiscard]] static auto create(std::ptrdiff_t n, Args&&... args)
        {
            return new(std::abs(n)) holder(n, std::forward<Args>(args)...);
        }
    };

public:
    using weak = object__weak;
    friend class object__weak;

    using atomic = object__atomic;
    friend class object__atomic;

    template<typename F>
    using fn = object__fn<F>;
    template<typename F>
    friend class object__fn;

    template<typename T>
    using ptr = object__ptr<T>;
    template<typename T>
    friend class object__ptr;

    template<typename T>
    using ref = object__ref<T>;
    template<typename T>
    friend class object__ref;

    template<typename T>
    using vec = object__vec<T>;
    template<typename T>
    friend class object__vec;

    template<typename T, typename U>
    using fam = object__fam<T, U>;
    template<typename T, typename U>
    friend class object__fam;

    template<typename CharT>
    using str = object__str<CharT>;
    template<typename CharT>
    friend class object__str;

    using ls = str<char>;
    using ws = str<wchar_t>;
    using u16s = str<char16_t>;
    using u32s = str<char32_t>;
#ifdef __cpp_char8_t
    using u8s = str<char8_t>;
#endif

    using handle = placeholder*;

    explicit object(handle p) noexcept : p(p) {}

    object() noexcept : p(nullptr) {}

    object(object&& obj) noexcept : p(obj.p)
    {
        obj.p = nullptr;
    }

    object(const object& obj) noexcept : p(obj.p)
    {
        if(p) p->addref();
    }

    ~object() noexcept
    {
        if(p && p->release() == 0)
            p->destroy();
    }

    object& operator=(const object& obj) noexcept
    {
        if(p != obj.p) object(obj).swap(*this);
        return *this;
    }

    object& operator=(object&& obj) noexcept
    {
        if(p != obj.p) object(std::move(obj)).swap(*this);
        return *this;
    }

    void swap(object& obj) noexcept
    {
        std::swap(p, obj.p);
    }

    explicit operator bool() const noexcept
    {
        return p != nullptr;
    }

    [[nodiscard]] type_index type() const noexcept
    {
        return p ? p->type() : null_t();
    }

    [[nodiscard]] handle release() noexcept
    {
        return std::exchange(p, nullptr);
    }

    bool operator==(const object& obj) const noexcept { return p == obj.p; }
    bool operator!=(const object& obj) const noexcept { return p != obj.p; }
    bool operator< (const object& obj) const noexcept { return p <  obj.p; }
    bool operator> (const object& obj) const noexcept { return p >  obj.p; }
    bool operator<=(const object& obj) const noexcept { return p <= obj.p; }
    bool operator>=(const object& obj) const noexcept { return p >= obj.p; }

public:
    template<typename ValueType, typename U = rmcvr<ValueType>, typename... Args>
    decltype(auto) emplace(Args&&... args)
    {
        auto q = holder<U>::create(std::forward<Args>(args)...);
        object old(std::exchange(p, q));
        return q->value();
    }

    template<typename ValueType, typename U = enable<ValueType>>
    object(ValueType&& value) : p(holder<U>::create(std::forward<ValueType>(value))) {}

    template<typename ValueType, typename... Args>
    object(std::in_place_type_t<ValueType>, Args&&... args)
        : p(holder<rmcvr<ValueType>>::create(std::forward<Args>(args)...)) {}

public:
    static object from(placeholder* p) noexcept
    {
        p->addref();
        return object(p);
    }

    template<typename T, typename... Ts>
    static std::enable_if_t<!std::is_base_of_v<placeholder, T>, holder<T, Ts...>*> from(const T* p) noexcept
    {
        auto h = static_cast<holder<T, Ts...>*>(nullptr);
        h = reinterpret_cast<holder<T, Ts...>*>(
            reinterpret_cast<unsigned char*>(const_cast<T*>(p)) -
            reinterpret_cast<std::ptrdiff_t>(std::addressof(h->value())));
        return h;
    }

public:
    template<typename ValueType>
    friend ValueType* unsafe_object_cast(object* obj) noexcept;

    template<typename ValueType>
    friend ValueType* object_cast(object* obj) noexcept;

    template<typename ValueType>
    friend ValueType* polymorphic_object_cast(object* obj) noexcept;
};

template<typename ValueType>
ValueType* unsafe_object_cast(object* obj) noexcept
{
    return std::addressof(static_cast<object::holder<object::rmcvr<ValueType>>*>(obj->p)->value());
}

template<typename ValueType>
ValueType* object_cast(object* obj) noexcept
{
    if(obj && obj->p && obj->p->type() == object::type_id<object::rmcvr<ValueType>>())
        return unsafe_object_cast<ValueType>(obj);
    return nullptr;
}

template<typename ValueType>
ValueType* polymorphic_object_cast(object* obj) noexcept
{
    try { if (obj && obj->p) obj->p->throws(); }
    catch (ValueType* p) { return p; }
    catch (...) {}
    return nullptr;
}


#define CAST(cast) \
template<typename ValueType> std::add_const_t<ValueType>* cast(const object* obj) noexcept \
{ return cast<ValueType>(const_cast<object*>(obj)); } \
template<typename ValueType> ValueType& cast(object& obj) \
{ if(auto p = cast<ValueType>(std::addressof(obj))) return *p; throw bad_object_cast{}; } \
template<typename ValueType> std::add_const_t<ValueType>& cast(const object& obj) \
{ if(auto p = cast<ValueType>(std::addressof(obj))) return *p; throw bad_object_cast{}; } \


CAST(unsafe_object_cast)
CAST(object_cast)
CAST(polymorphic_object_cast)
#undef CAST


class object__weak
{
    using handle = object::handle;
    using placeholder = object::placeholder;
    placeholder* p;

public:
    object__weak() noexcept : p(nullptr) {}
    explicit object__weak(handle p) noexcept : p(p) {}
    object__weak(const object& obj) noexcept : p(obj.p) { if (p) p->weak.addref(); }
    object__weak(const object__weak& w) noexcept : p(w.p) { if (p) p->weak.addref(); }
    object__weak(object__weak&& w) noexcept : p(w.p) { w.p = nullptr; }
    ~object__weak() { if (p && p->weak.release() == 0) delete p; }
    void swap(object__weak& w) noexcept { std::swap(p, w.p); }
    object__weak& operator=(const object__weak& w) noexcept { if (p != w.p) object__weak(w).swap(*this); return *this; }
    object__weak& operator=(object__weak&& w) noexcept { if (p != w.p) object__weak(std::move(w)).swap(*this); return *this; }

    bool operator==(const object__weak& w) const noexcept { return p == w.p; }
    bool operator!=(const object__weak& w) const noexcept { return p != w.p; }
    bool operator< (const object__weak& w) const noexcept { return p <  w.p; }
    bool operator> (const object__weak& w) const noexcept { return p >  w.p; }
    bool operator<=(const object__weak& w) const noexcept { return p <= w.p; }
    bool operator>=(const object__weak& w) const noexcept { return p >= w.p; }

    explicit operator bool() const noexcept { return p != nullptr; }
    bool expired() const noexcept { return !p || p->count() <= 0; }
    object lock() const noexcept { return object(p && p->xref() ? p : nullptr); }

#if defined(__cpp_lib_atomic_wait)
    void wait_until_expired() const noexcept
    {
        if (!p) return;
        while (true)
        {
            auto c = p->refcount.load(std::memory_order_relaxed);
            if (c <= 0) break;
            p->refcount.wait(c, std::memory_order_relaxed);
        }
    }
#endif

    [[nodiscard]] operator object() const
    {
        object obj = lock();
        if (!obj) throw bad_weak_object{};
        return obj;
    }

    [[nodiscard]] handle release() noexcept
    {
        return std::exchange(p, nullptr);
    }

    static object__weak from(placeholder* p) noexcept
    {
        p->weak.addref();
        return object__weak(p);
    }

    template<typename T, typename... Ts>
    static std::enable_if_t<!std::is_base_of_v<placeholder, T>, object__weak> from(const T* p) noexcept
    {
        return from(object::from<T, Ts...>(p));
    }
};

class object__atomic
{
    using handle = object::handle;
    using placeholder = object::placeholder;
    using storage_t = std::uintptr_t;
    enum : storage_t
    {
        mask = 3,
        locked = 1,
        waiting = 2,
        condition = 3,
    };

    mutable std::atomic<storage_t> storage;
    static_assert(sizeof(handle) <= sizeof(storage_t), "atomic::storage cannot hold object::handle");
    static_assert(alignof(placeholder) >= (1 << 2), "2 low order bits are needed by object::atomic");

    handle lock_and_load(std::memory_order order) const noexcept
    {
        auto v = storage.load(order);
        while (true) switch (v & mask)
        {
            case 0: // try to lock
            case condition: // cv wait
                if (storage.compare_exchange_weak(v, (v & ~mask) | locked,
                    std::memory_order_acquire, std::memory_order_relaxed))
                    return reinterpret_cast<handle>(v & ~mask);
                break;
            case locked: // try to wait
                if (!storage.compare_exchange_weak(v, (v & ~mask) | waiting,
                    std::memory_order_relaxed, std::memory_order_relaxed))
                    break; // try again
                v = (v & ~mask) | waiting;
                [[fallthrough]];
            case waiting: // just wait
#if defined(__cpp_lib_atomic_wait)
                storage.wait(v, std::memory_order_relaxed);
#endif
                v = storage.load(std::memory_order_relaxed);
                break;
            default:
                std::terminate();
        }
    }

    void store_and_unlock(handle h, std::memory_order order) const noexcept
    {
        auto v = storage.exchange(reinterpret_cast<storage_t>(h), order);
#if defined(__cpp_lib_atomic_wait)
        // notify all waiters since waiting mask has been cleared.
        if ((v & mask) == waiting) storage.notify_all();
#else
        (void)v;
#endif
    }

public:
    static constexpr bool is_always_lock_free = false;
    bool is_lock_free() const noexcept { return is_always_lock_free; }

    object__atomic(const object__atomic&) = delete;
    object__atomic& operator=(const object__atomic&) = delete;
    object__atomic() noexcept : storage(storage_t{}) {}

    object__atomic(object obj) noexcept
        : storage(reinterpret_cast<storage_t>(obj.release())) {}

    ~object__atomic() noexcept
    {
        auto v = storage.load(std::memory_order_relaxed) & ~mask;
        (void)object(reinterpret_cast<handle>(v));
    }

    operator object() const noexcept
    {
        return load();
    }

    object operator=(object desired) noexcept
    {
        store(desired);
        return std::move(desired);
    }

    void store(object desired, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        exchange(std::move(desired), order);
    }

    object load(std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        if (order != std::memory_order_seq_cst) order = std::memory_order_relaxed;
        object obj(lock_and_load(order));
        if (obj.p) obj.p->addref();
        store_and_unlock(obj.p, std::memory_order_release);
        return obj;
    }

    object exchange(object desired, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        if (order != std::memory_order_seq_cst) order = std::memory_order_release;
        object obj(lock_and_load(std::memory_order_relaxed));
        store_and_unlock(desired.release(), order);
        return obj;
    }

    bool compare_exchange_weak(object& expected, object desired,
                               std::memory_order success,
                               std::memory_order failure) noexcept
    {
        return compare_exchange_strong(expected, std::move(desired), success, failure);
    }

    bool compare_exchange_weak(object& expected, object desired,
                               std::memory_order order =
                               std::memory_order_seq_cst) noexcept
    {
        return compare_exchange_strong(expected, std::move(desired), order);
    }

    bool compare_exchange_strong(object& expected, object desired,
                                 std::memory_order success,
                                 std::memory_order failure) noexcept
    {
        if (success != std::memory_order_seq_cst) success = std::memory_order_release;
        if (failure != std::memory_order_seq_cst) failure = std::memory_order_release;
        object obj(lock_and_load(std::memory_order_relaxed));
        if (obj == expected)
        {
            store_and_unlock(desired.release(), success);
            return true;
        }
        else
        {
            if (obj.p) obj.p->addref();
            store_and_unlock(obj.p, failure);
            obj.swap(expected);
            return false;
        }
    }

    bool compare_exchange_strong(object& expected, object desired,
                                 std::memory_order order =
                                 std::memory_order_seq_cst) noexcept
    {
        return compare_exchange_strong(expected, std::move(desired), order, order);
    }

#if defined(__cpp_lib_atomic_wait)
    void wait(object old, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        auto o = reinterpret_cast<storage_t>(old.p);
        auto v = o;
        while (o == (v & ~mask))
        {
            storage.wait(v, order);
            v = storage.load(std::memory_order_relaxed);
        }
    }

    void notify_one() noexcept
    {
        return storage.notify_one();
    }

    void notify_all() noexcept
    {
        return storage.notify_all();
    }
#endif

    //////////////////
    //// spinlock ////
    //////////////////
    bool try_lock() noexcept
    {
        auto v = storage.load(std::memory_order_relaxed);
        if ((v & mask) != 0 && (v & mask) != condition) return false;
        return storage.compare_exchange_weak(v, (v & ~mask) | locked,
               std::memory_order_acquire, std::memory_order_relaxed);
    }

    void lock() noexcept
    {
        (void)lock_and_load(std::memory_order_relaxed);
    }

    void unlock() noexcept
    {
        auto v = storage.load(std::memory_order_relaxed) & ~mask;
        store_and_unlock(reinterpret_cast<handle>(v), std::memory_order_release);
    }

#if defined(__cpp_lib_atomic_wait)
    /**
     * @brief Unlike std::condition_variable, @p lock() must be held while calling @p notify_*().
     *
     * @code
     * //////////////////
     * //// Thread A ////
     * //////////////////
     * obj.lock();
     * while (condition is false)
     *     obj.wait();
     * // Perform action appropriate to condition
     * obj.unlock();
     *
     * //////////////////
     * //// Thread B ////
     * //////////////////
     * obj.lock();
     * // Set condition to true
     * obj.notify_one();
     * obj.unlock();
     * @endcode
     */
    void wait() noexcept
    {
        auto v = storage.load(std::memory_order_relaxed);
        assert((v & mask) != 0 && (v & mask) != condition && "wait() is called while lock() is not held!");
        store_and_unlock(reinterpret_cast<handle>((v & ~mask) | condition), std::memory_order_release);
        storage.wait((v & ~mask) | condition, std::memory_order_relaxed);
        (void)lock_and_load(std::memory_order_relaxed);
    }

    template<typename Predicate>
    void wait(Predicate stop_waiting) noexcept(std::is_nothrow_invocable_v<Predicate>)
    {
        while (!stop_waiting()) wait();
    }
#endif

    object get() const noexcept
    {
        auto v = storage.load(std::memory_order_relaxed);
        object obj(reinterpret_cast<handle>(v & ~mask));
        if (obj.p) obj.p->addref();
        return obj;
    }

    object set(object obj) noexcept
    {
        auto o = reinterpret_cast<storage_t>(obj.p);
        auto v = storage.load(std::memory_order_relaxed);
        while (!storage.compare_exchange_weak(v, o | (v & mask), std::memory_order_relaxed));
        obj.p = reinterpret_cast<handle>(v & ~mask);
        return std::move(obj);
    }
};

template<typename R, typename... Args>
class object__fn<R(Args...)> : public object
{
    template<typename F>
    using enable = std::enable_if_t<!std::is_base_of_v<object__fn, F> &&
                                    std::is_invocable_r_v<R, F, Args...>>;
public:
    object__fn() noexcept = default;

    template<typename T, typename F = typename is_in_place_type<std::decay_t<T>>::type,
             typename = enable<F>, typename... A>
    object__fn(T&& t, A&&... a) : object(std::in_place_type<R(Args...)>, std::forward<T>(t), std::forward<A>(a)...) {}

    template<typename Object, typename = std::enable_if_t<std::is_same_v<rmcvr<Object>, object>>>
    object__fn(Object&& obj)
    {
        if (obj && obj.type() != type_id<R(Args...)>()) throw object_not_fn{};
        object::operator=(std::forward<Object>(obj));
    }

    void swap(object__fn& f) noexcept
    {
        return object::swap(f);
    }

    template<typename T, typename F = std::decay_t<T>,
            typename = std::enable_if_t<!is_in_place_type<F>::value>,
            typename = enable<F>, typename... A>
    decltype(auto) emplace(A&&... a)
    {
        return object::emplace<R(Args...)>(std::in_place_type<F>, std::forward<A>(a)...);
    }

    R operator()(Args... args) const
    {
        if (p == nullptr) throw object_not_fn{};
        return static_cast<holder<R(Args...)>*>(p)->call(std::forward<Args>(args)...);
    }
};

template<typename R, typename... Args>
class object__fn<R(&)(Args...)>         // std::function_ref
{
    void* o;
    R (*f)(void*, Args...);

    static R callobj(void* o, Args... args)
    {
        return static_cast<R>((*static_cast<object__fn<R(Args...)>*>(o))(
                std::forward<Args>(args)...));
    }

public:
    object__fn(const object__fn<R(Args...)>& f) : o((void*)std::addressof(f)), f(&callobj)
    {
        if (!f) throw object_not_fn{};
    }

    template<typename F, typename = std::enable_if_t<!std::is_base_of_v<object__fn<R(Args...)>, object::rmcvr<F>> &&
                                                     !std::is_base_of_v<object__fn<R(&)(Args...)>, object::rmcvr<F>> &&
                                                     std::is_invocable_r_v<R, F, Args...>>>
    object__fn(F&& f) noexcept : o((void*)std::addressof(f))
    {
        this->f = [](void* o, Args... args) -> R
        {
            return static_cast<R>((*(std::add_pointer_t<F>)(o))(
                    std::forward<Args>(args)...));
        };
    }

    template<typename Object, typename = std::enable_if_t<std::is_same_v<Object, object>>>
    object__fn(const Object& obj) : o((void*)std::addressof(obj)), f(&callobj)
    {
        if (obj.type() != object::type_id<R(Args...)>()) throw object_not_fn{};
    }

    object__fn<R(Args...)> object() const noexcept
    {
        return f == &callobj ? *static_cast<object__fn<R(Args...)>*>(o) : object__fn<R(Args...)>{};
    }

    R operator()(Args... args) const
    {
        return (*f)(o, std::forward<Args>(args)...);
    }
};

template<typename T>
class object__ptr : public object
{
    template<typename U>
    friend class object__ptr;

    template<typename U>
    friend class object__ref;

protected:
    T* p;

public:
    object__ptr() noexcept : p(nullptr) {}

    object__ptr(const object& obj)
    {
        p = obj ? std::addressof(const_cast<T&>(object_cast<T>(obj))) : nullptr;
        object::operator=(obj);
    }

    object__ptr(object&& obj)
    {
        p = obj ? std::addressof(object_cast<T>(obj)) : nullptr;
        object::swap(obj);
    }

    object__ptr(const object& obj, T* p)
    {
        if (p) this->p = p;
        else if (obj) this->p = std::addressof(const_cast<T&>(polymorphic_object_cast<T>(obj)));
        else this->p = nullptr;
        object::operator=(obj);
    }

    object__ptr(object&& obj, T* p)
    {
        if (p) this->p = p;
        else if (obj) this->p = std::addressof(polymorphic_object_cast<T>(obj));
        else this->p = nullptr;
        object::swap(obj);
    }

    template<typename U>
    object__ptr(const object__ptr<U>& p) noexcept : object(p), p(p.p) {}

    template<typename U>
    object__ptr(object__ptr<U>&& p) noexcept : object(std::move(p)), p(p.p) {}

    void swap(object__ptr& p) noexcept
    {
        std::swap(this->p, p.p);
        return object::swap(p);
    }

    template<typename... Args>
    T& emplace(Args&&... args)
    {
        return *(p = std::addressof(object::emplace<T>(std::forward<Args>(args)...)));
    }

    explicit operator bool() const noexcept
    {
        return p != nullptr;
    }

    T* operator->()
    {
        if (p == nullptr) throw bad_object_cast{};
        return p;
    }

    const T* operator->() const
    {
        if (p == nullptr) throw bad_object_cast{};
        return p;
    }

    [[nodiscard]] T& operator*()
    {
        return *operator->();
    }

    [[nodiscard]] const T& operator*() const
    {
        return *operator->();
    }

    static object__ptr from(const T* p) noexcept
    {
        object__ptr r;
        r.p = const_cast<T*>(p);
        if (p) object::from(object::from(p)).swap(r);
        return r;
    }
};

template<typename T>
class object__ref : public object
{
    template<typename U>
    friend class object__ptr;

    template<typename U>
    friend class object__ref;

protected:
    T* p;

public:
    object__ref(const object& obj)
    {
        p = std::addressof(const_cast<T&>(object_cast<T>(obj)));
        object::operator=(obj);
    }

    object__ref(object&& obj)
    {
        p = std::addressof(object_cast<T>(obj));
        object::swap(obj);
    }

    object__ref(const object& obj, T* p)
    {
        this->p = p ? p : std::addressof(const_cast<T&>(polymorphic_object_cast<T>(obj)));
        object::operator=(obj);
    }

    object__ref(object&& obj, T* p)
    {
        this->p = p ? p : std::addressof(polymorphic_object_cast<T>(obj));
        object::swap(obj);
    }

    template<typename U>
    object__ref(const object__ref<U>& r) noexcept : object(r), p(r.p) {}

    template<typename U>
    object__ref(object__ref<U>&& r) noexcept : object(std::move(r)), p(r.p) {}

    template<typename U>
    object__ref(const object__ptr<U>& p) : p(p.p)
    {
        if (!p) throw bad_object_cast{};
        object::operator=(p);
    }

    template<typename U>
    object__ref(object__ptr<U>&& p) : p(p.p)
    {
        if (!p) throw bad_object_cast{};
        object::swap(p);
    }

    void swap(object__ref& r) noexcept
    {
        std::swap(p, r.p);
        return object::swap(r);
    }

    [[nodiscard]] handle release() const noexcept
    {
        return object(*this).release();
    }

    template<typename... Args>
    T& emplace(Args&&... args)
    {
        return *(p = std::addressof(object::emplace<T>(std::forward<Args>(args)...)));
    }

    explicit operator bool() const noexcept
    {
        return p != nullptr;
    }

    [[nodiscard]] object__ptr<T> operator&() const noexcept
    {
        return object__ptr<T>(*this, p);
    }

    operator T&() noexcept
    {
        return *p;
    }

    operator const T&() const noexcept
    {
        return *p;
    }

    T* operator->() noexcept
    {
        return p;
    }

    const T* operator->() const noexcept
    {
        return p;
    }

#ifdef OBJECT_HAVE_OPERATOR_DOT // someday we could have this
    T& operator.() noexcept
    {
        return *this;
    }

    const T& operator.() const noexcept
    {
        return *this;
    }
#endif

    static object__ref from(const T& t) noexcept
    {
        object__ref r;
        r.p = const_cast<T*>(std::addressof(t));
        object::from(object::from(r.p)).swap(r);
        return r;
    }
};

template<typename T>
class object__vec<T&>         // std::span
{
public:
    using element_type     = T;
    using value_type       = std::remove_cv_t<T>;
    using size_type        = std::size_t;
    using difference_type  = std::ptrdiff_t;
    using pointer          = element_type*;
    using const_pointer    = const element_type*;
    using reference        = element_type&;
    using const_reference  = const element_type&;
    using iterator         = pointer;
//  using reverse_iterator = std::reverse_iterator<iterator>;

    template<typename R>
    object__vec(R&& r) noexcept : p(std::data(r)), n(std::size(r)) {}

    object__vec() noexcept : p(nullptr), n(0) {}
    object__vec(pointer p, size_type n) noexcept : p(p), n(n) {}
    pointer data() const noexcept { return p; }
    size_type size() const noexcept { return n; }
    bool empty() const noexcept { return n == 0; }
    iterator begin() const noexcept { return p; }
    iterator end() const noexcept { return p + n; }
    reference front() const noexcept { return p[0]; }
    reference back() const noexcept { return p[n - 1]; }
    reference operator[](size_type i) const noexcept { return p[i]; }
    size_type size_bytes() const noexcept { return n * sizeof(element_type); }
    object__vec subspan(size_type offset, size_type count) const noexcept { return {p + offset, count}; }
    object__vec first(size_type count) const noexcept { return {p, count}; }
    object__vec last(size_type count) const noexcept { return {p + (n - count), count}; }

private:
    pointer p;
    size_type n;
};

template<typename T>
class object__vec : public object
{
public:
    object__vec() = default;

    explicit object__vec(std::ptrdiff_t n)
    {
        if (n != 0) object::emplace<T[]>(n);
    }

    object__vec(const object& obj)
    {
        if (obj && obj.type() != type_id<T[]>()) throw bad_object_cast{};
        object::operator=(obj);
    }

    object__vec(object&& obj)
    {
        if (obj && obj.type() != type_id<T[]>()) throw bad_object_cast{};
        object::swap(obj);
    }

    template<typename InputIt, typename Size>
    object__vec(InputIt first, Size count)
    {
        if (count > 0)
        {
            std::copy_n(first, count, object::emplace<T[]>(-static_cast<std::ptrdiff_t>(count)));
        }
    }

    object__vec(std::initializer_list<T> list) : object__vec(list.begin(), list.size())
    {
    }

    void swap(object__vec& v) noexcept
    {
        return object::swap(v);
    }

    object__vec<T&> emplace(std::ptrdiff_t n)
    {
        if (n != 0) object::emplace<T[]>(n);
        else object().swap(*this);
        return *this;
    }

    operator object__vec<T&>() noexcept
    {
        if (auto h = static_cast<holder<T[]>*>(p))
            return { h->value(), static_cast<std::size_t>(h->length()) };
        return {};
    }

    operator object__vec<const T&>() const noexcept
    {
        return const_cast<object__vec*>(this)->operator object__vec<T&>();
    }

    T* data() noexcept
    {
        if (auto h = static_cast<holder<T[]>*>(p))
            return h->value();
        return nullptr;
    }

    const T* data() const noexcept
    {
        return const_cast<object__vec*>(this)->data();
    }

    std::size_t size() const noexcept
    {
        if (auto h = static_cast<holder<T[]>*>(p))
            return static_cast<std::size_t>(h->length());
        return 0;
    }

    bool empty() const noexcept
    {
        return p == nullptr;
    }

    T& operator[](std::size_t i) noexcept
    {
        return static_cast<holder<T[]>*>(p)->value()[i];
    }

    const T& operator[](std::size_t i) const noexcept
    {
        return const_cast<object__vec*>(this)->operator[](i);
    }

    T& at(std::size_t i)
    {
        if (size() <= i) throw std::out_of_range("object::vec::at()");
        return static_cast<holder<T[]>*>(p)->value()[i];
    }

    const T& at(std::size_t i) const
    {
        return const_cast<object__vec*>(this)->at(i);
    }

    T& front() noexcept
    {
        return *data();
    }

    T& back() noexcept
    {
        return *(data() + size() - 1);
    }

    const T& front() const noexcept
    {
        return *data();
    }

    const T& back() const noexcept
    {
        return *(data() + size() - 1);
    }

    T* begin() noexcept
    {
        if (auto h = static_cast<holder<T[]>*>(p))
            return h->value();
        return nullptr;
    }

    T* end() noexcept
    {
        if (auto h = static_cast<holder<T[]>*>(p))
            return h->value() + h->length();
        return nullptr;
    }

    const T* begin() const noexcept
    {
        return const_cast<object__vec*>(this)->begin();
    }

    const T* end() const noexcept
    {
        return const_cast<object__vec*>(this)->end();
    }
};

template<typename T, typename U>
class object__fam : public object__ptr<T>         // flexible array member
{
public:
    object__fam() = default;

    template<typename... Args>
    explicit object__fam(std::ptrdiff_t n, Args&&... args)
    {
        auto h = object::holder<T, U[]>::create(n, std::forward<Args>(args)...);
        object::p = h;
        object__ptr<T>::p = std::addressof(h->value());
    }

    object__fam(const object& obj)
    {
        auto h = obj ? dynamic_cast<object::holder<T, U[]>*>(obj.p) : nullptr;
        if (obj && !h) throw bad_object_cast{};
        object::operator=(obj);
        object__ptr<T>::p = std::addressof(h->value());
    }

    object__fam(object&& obj)
    {
        auto h = obj ? dynamic_cast<object::holder<T, U[]>*>(obj.p) : nullptr;
        if (obj && !h) throw bad_object_cast{};
        object::swap(obj);
        object__ptr<T>::p = std::addressof(h->value());
    }

    void swap(object__fam& f) noexcept
    {
        return object__ptr<T>::swap(f);
    }

    template<typename... Args>
    T& emplace(std::ptrdiff_t n, Args&&... args)
    {
        object__fam(n, std::forward<Args>(args)...).swap(*this);
        return object__ptr<T>::operator*();
    }

    object__vec<U&> array() noexcept
    {
        if (auto h = static_cast<object::holder<T, U[]>*>(object::p))
            return { h->array(), static_cast<std::size_t>(h->length()) };
        return {};
    }

    object__vec<const U&> array() const noexcept
    {
        return const_cast<object__fam*>(this)->array();
    }

    static object__vec<U&> array(T* p) noexcept
    {
        if (!p) return {};
        auto h = object::from<T, U[]>(p);
        return { h->array(), static_cast<std::size_t>(h->length()) };
    }

    static object__vec<const U&> array(const T* p) noexcept
    {
        return array(const_cast<T*>(p));
    }

    static object__fam from(const T* p) noexcept
    {
        object__fam r;
        object__ptr<T>::from(p).swap(r);
        return r;
    }
};

template<typename CharT>
class object__str         // ATL::CStringT
{
    CharT* p;

    ::object::holder<CharT[]>* storage() const noexcept
    {
        return ::object::from<CharT[]>(reinterpret_cast<CharT(*)[]>(p));
    }

public:
    object__vec<CharT> object() const noexcept
    {
        object__vec<CharT> v;
        if (p) (v.p = storage())->addref();
        return v;
    }

    object__str(const class object& obj) : p(nullptr)
    {
        object__vec<CharT> v(obj);
        if (!v.empty() && v.back() != CharT{})
            throw bad_object_cast{};
        p = v.data();
        (void)v.release();
    }

    object__str(class object&& obj)
    {
        object__vec<CharT> v(std::move(obj));
        if (!v.empty() && v.back() != CharT{})
        {
            obj.swap(v);
            throw bad_object_cast{};
        }
        p = v.data();
        (void)v.release();
    }

    object__str() noexcept : p(nullptr) {}
    object__str(std::nullptr_t) noexcept : object__str() {}
    object__str(object__str&& s) noexcept : p(s.p) { s.p = nullptr; }
    object__str(const object__str& s) noexcept : p(s.p) { (void)object().release(); }
    ~object__str() { if (p) (void)(class object)((object::handle)storage()); }
    void swap(object__str& s) noexcept { std::swap(p, s.p); }

    object__str& operator=(std::nullptr_t) noexcept { object__str().swap(*this); return *this; }
    object__str& operator=(const object__str& s) noexcept { if (p != s.p) object__str(s).swap(*this); return *this; }
    object__str& operator=(object__str&& s) noexcept { if (p != s.p) object__str(std::move(s)).swap(s); return *this; }

    bool operator==(const object__str& s) const noexcept { return p == s.p; }
    bool operator!=(const object__str& s) const noexcept { return p != s.p; }
    bool operator< (const object__str& s) const noexcept { return p <  s.p; }
    bool operator> (const object__str& s) const noexcept { return p >  s.p; }
    bool operator<=(const object__str& s) const noexcept { return p <= s.p; }
    bool operator>=(const object__str& s) const noexcept { return p >= s.p; }

    [[nodiscard]] operator class object() const noexcept { return object(); }
    [[nodiscard]] operator const CharT*() const noexcept { return p; }
    [[nodiscard]] operator CharT*() noexcept { return p; }

    explicit operator bool() const noexcept { return p != nullptr; }
    std::size_t size() const noexcept { return p ? storage()->length() - 1 : 0; }
    std::size_t length() const noexcept { return size(); }
    bool empty() const noexcept { return size() == 0; }
    const CharT* data() const noexcept { return p; }
    CharT* data() noexcept { return p; }
    CharT* begin() noexcept { return data(); }
    CharT* end() noexcept { return data() + size(); }
    const CharT* begin() const noexcept { return data(); }
    const CharT* end() const noexcept { return data() + size(); }

    const CharT* c_str() const noexcept
    {
        static const CharT null{};
        return p ? p : std::addressof(null);
    }

    object__str(std::size_t count, CharT ch)
    {
        object__vec<CharT> v(-static_cast<std::ptrdiff_t>(count + 1));
        std::fill_n(v.data(), count, ch);
        v.back() = CharT{};
        p = v.data();
        (void)v.release();
    }

    template<typename Traits>
    object__str(std::basic_string_view<CharT, Traits> s) : p(nullptr)
    {
        object__vec<CharT> v(-static_cast<std::ptrdiff_t>(s.size() + 1));
        std::copy_n(s.data(), s.size(), v.data());
        v.back() = CharT{};
        p = v.data();
        (void)v.release();
    }

    template<typename Traits>
    object__str(const std::basic_string<CharT, Traits>& s)
        : object__str(std::basic_string_view<CharT, Traits>(s))
    {
    }

    object__str(const CharT* s) : object__str(std::basic_string_view<CharT>(s))
    {
    }

    template<typename Traits>
    object__str& operator=(std::basic_string_view<CharT, Traits> s)
    {
        object__str(s).swap(*this);
        return *this;
    }

    template<typename Traits>
    object__str& operator=(const std::basic_string<CharT, Traits>& s)
    {
        object__str(s).swap(*this);
        return *this;
    }

    object__str& operator=(const CharT* s)
    {
        object__str(s).swap(*this);
        return *this;
    }

    template<typename Traits>
    [[nodiscard]] operator std::basic_string_view<CharT, Traits>() const noexcept
    {
        if (p) return { data(), size() };
        else return {};
    }
};


#if defined(__cpp_lib_ranges)
#include <ranges>
template<typename T> inline constexpr bool std::ranges::enable_borrowed_range<object::vec<T&>> = true;
template<typename T> inline constexpr bool std::ranges::enable_view<object::vec<T>> = true;
template<typename T> inline constexpr bool std::ranges::enable_view<object::str<T>> = true;
#endif


#ifdef cobject_handle_copy
#undef cobject_handle_copy
#endif
#ifdef cobject_handle_clear
#undef cobject_handle_clear
#endif
#define cobject_handle_copy(p) (void*)object((const object&)(p)).release()
#define cobject_handle_clear(p) (void)object((object::handle)(p))


#endif //OBJECT_HPP
