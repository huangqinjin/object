#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <type_traits>
#include <atomic>
#include <typeinfo>
#include <utility>        // move, exchange

class bad_object_cast : public std::exception {};

class object
{
    template<typename T>
    using rmcvr = std::remove_cv_t<std::remove_reference_t<T>>;

    template<typename T, typename U = rmcvr<T>>
    using enable = std::enable_if_t<!std::is_same_v<object, U>, U>;

    class placeholder
    {
        std::atomic<long> refcount = ATOMIC_VAR_INIT(1);

    public:
        long addref(long c = 1) noexcept { return c + refcount.fetch_add(c, std::memory_order_relaxed); }
        long release(long c = 1) noexcept { return addref(-c); }
        virtual ~placeholder() = default;
        virtual const std::type_info& type() const noexcept = 0;
    } *p;

    template<typename T>
    class holder : public placeholder
    {
        T v;
        const std::type_info& type() const noexcept final { return typeid(T); }

        template<typename... Args>
        explicit holder(std::true_type, Args&&... args) : v(std::forward<Args>(args)...) {}

        template<typename... Args>
        explicit holder(std::false_type, Args&&... args) : v{std::forward<Args>(args)...} {}

    public:
        T& value() noexcept
        {
            return v;
        }

        template<typename... Args>
        static auto create(Args&&... args)
        {
            return new holder(std::is_constructible<T, Args&&...>{}, std::forward<Args>(args)...);
        }
    };

    explicit object(placeholder* p) noexcept : p(p) {}

public:
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
            delete p;
    }

    object exchange(const object& obj) noexcept
    {
        object old(std::exchange(p, obj.p));
        if(p) p->addref();
        return old;
    }

    object exchange(object&& obj) noexcept
    {
        object old(std::exchange(p, obj.p));
        obj.p = nullptr;
        return old;
    }

    object& operator=(const object& obj) noexcept
    {
        if(p != obj.p) exchange(obj);
        return *this;
    }

    object& operator=(object&& obj) noexcept
    {
        if(p != obj.p) exchange(std::move(obj));
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

    const std::type_info& type() const noexcept
    {
        return p ? p->type() : typeid(void);
    }

    bool operator==(const object& obj) const noexcept { return p == obj.p; }
    bool operator!=(const object& obj) const noexcept { return p != obj.p; }
    bool operator< (const object& obj) const noexcept { return p <  obj.p; }
    bool operator> (const object& obj) const noexcept { return p >  obj.p; }
    bool operator<=(const object& obj) const noexcept { return p <= obj.p; }
    bool operator>=(const object& obj) const noexcept { return p >= obj.p; }

public:
    template<typename ValueType, typename U = enable<ValueType>, typename... Args>
    decltype(auto) emplace(Args&&... args)
    {
        auto q = holder<U>::create(std::forward<Args>(args)...);
        object old(std::exchange(p, q));
        return q->value();
    }

    template<typename ValueType, typename U = enable<ValueType>>
    object(ValueType&& value) : p(holder<U>::create(std::forward<ValueType>(value))) {}

    template<typename ValueType, typename U = enable<ValueType>>
    object& operator=(ValueType&& value)
    {
        object(std::forward<ValueType>(value)).swap(*this);
        return *this;
    }

public:
    template<typename ValueType>
    friend const ValueType* unsafe_object_cast(const object* obj) noexcept;

    template<typename ValueType>
    friend const ValueType* object_cast(const object* obj) noexcept;
};


template<typename ValueType>
const ValueType* unsafe_object_cast(const object* obj) noexcept
{
    return std::addressof(static_cast<object::holder<object::rmcvr<ValueType>>*>(obj->p)->value());
}

template<typename ValueType>
const ValueType* object_cast(const object* obj) noexcept
{
    if(obj && obj->p && obj->p->type() == typeid(object::rmcvr<ValueType>))
        return unsafe_object_cast<ValueType>(obj);
    return nullptr;
}


#define CAST(cast) \
template<typename ValueType> ValueType* cast(object* obj) noexcept \
{ return (ValueType*)(cast<ValueType>(const_cast<const object*>(obj))); } \
template<typename ValueType> ValueType& cast(object& obj) \
{ if(auto p = cast<ValueType>(std::addressof(obj))) return *p; throw bad_object_cast{}; } \
template<typename ValueType> const ValueType& cast(const object& obj) \
{ if(auto p = cast<ValueType>(std::addressof(obj))) return *p; throw bad_object_cast{}; } \


CAST(unsafe_object_cast)
CAST(object_cast)
#undef CAST


#endif //OBJECT_HPP
