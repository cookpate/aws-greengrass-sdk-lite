
#include <ggl/buffer.hpp>
#include <ggl/list.hpp>
#include <ggl/map.hpp>
#include <ggl/object.hpp>
#include <ggl/types.hpp>
#include <type_traits>
#include <utility>

namespace ggl {

template <ggl::ObjectType... Ts> class StackList;
template <ggl::ObjectType... Ts> class StackMap;

template <class T>
concept SubobjectType = std::disjunction_v<
    std::is_constructible<ggl::List, T>,
    std::is_constructible<ggl::Map, T>>;

template <class... Ts> class SubobjectHelper;

template <> class SubobjectHelper<> {
public:
    constexpr SubobjectHelper() noexcept = default;
};

template <SubobjectType THead, class... TRest>
class SubobjectHelper<THead, TRest...> : SubobjectHelper<TRest...> {
    using Parent = SubobjectHelper<TRest...>;

    THead head;

public:
    constexpr SubobjectHelper() noexcept = default;

    constexpr SubobjectHelper(THead &&head, TRest &&...rest) noexcept
        : Parent { std::forward<TRest>(rest)... }
        , head { std::forward<THead>(head) } {
    }
};

// Scalar types do not need subobject storage
template <class TIgnore, class... TRest>
class SubobjectHelper<TIgnore, TRest...> : SubobjectHelper<TRest...> {
    using Parent = SubobjectHelper<TRest...>;

public:
    constexpr SubobjectHelper() noexcept = default;

    constexpr SubobjectHelper(TIgnore && /*unused*/, TRest &&...rest) noexcept
        : Parent { std::forward<TRest>(rest)... } {
    }
};

template <ggl::ObjectType... Ts> class StackList : SubobjectHelper<Ts...> {
    std::array<ggl::Object, sizeof...(Ts)> list;
    using Parent = SubobjectHelper<Ts...>;

public:
    constexpr StackList(Ts &&...objs) noexcept
        : Parent { std::forward<Ts>(objs)... }
        , list { ggl::Object { objs }... } {
    }

    constexpr operator ggl::List() noexcept {
        return { list.data(), list.size() };
    }

    operator ggl::Object() noexcept {
        return ggl::Object { ggl::List { *this } };
    }
};

template <class T> class StackKV {
    std::string_view key;
    T val;

public:
    constexpr StackKV(std::string_view key, T &&value) noexcept
        : key { key }
        , val { std::forward<T>(value) } {
    }

    constexpr auto value() const noexcept {
        return val;
    }

    operator ggl::KV() noexcept {
        return ggl::KV { key, ggl::Object { val } };
    }
};

template <ggl::ObjectType... Ts> class StackMap : SubobjectHelper<Ts...> {
    std::array<ggl::KV, sizeof...(Ts)> map;

    using Parent = SubobjectHelper<Ts...>;

public:
    constexpr StackMap(StackKV<Ts> &&...objs) noexcept
        : Parent { (objs.value())... }
        , map { ggl::KV { objs }... } {
    }

    constexpr operator ggl::Map() noexcept {
        return { map.data(), map.size() };
    }

    operator ggl::Object() noexcept {
        return ggl::Object { ggl::Map { *this } };
    }
};

template <ggl::ObjectType... Ts>
StackMap(StackKV<Ts> &&...objs) -> StackMap<Ts...>;
template <ggl::ObjectType... Ts> StackList(Ts &&...objs) -> StackList<Ts...>;

namespace {
    StackList x { "24", true, 3.4 };
}

namespace {
    StackMap abc { StackKV { "24",
                             StackList { StackList { 5, 4.0, 3 },
                                         StackMap { StackKV { "24", 20 },
                                                    StackKV { "a", 21 } } } } };
}

constexpr size_t big_size = sizeof(abc);

}

int main() {
}
