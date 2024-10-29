#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>

namespace matter {

consteval size_t naive_digits10(size_t n) {
    using L = std::numeric_limits<size_t>;
    if (n >= L::max() / 10) return L::digits10 + 1;
    size_t log = 1, p = 10;
    while (p <= n) {
        p *= 10;
        log += 1;
    }
    return log;
}

template <size_t N>
struct StringLiteral {
    // this field needs to be public
    // because the type needs to be structural
    char _data[N];

    consteval size_t size() const { return N; }
    consteval const char *data() const { return _data; }

    consteval StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, _data);
    }

    template <size_t KOffset>
    consteval StringLiteral<N - KOffset> offset() const {
        static_assert(KOffset < N, "offset too big");
        char buf[N - KOffset] = {0};
        std::copy_n(_data + KOffset, N - KOffset, buf);
        return {buf};
    }

    template <size_t M>
    consteval StringLiteral<N + M - 1> operator+(StringLiteral<M> other) const {
        char buf[N + M - 1] = {0};
        auto it = std::copy_n(_data, N - 1, buf);
        std::copy_n(other._data, M, it);
        return {buf};
    }

    consteval size_t hash() const {
        // TODO: stronger fast consteval hash
        size_t hash = 0;
        for (char c : _data) hash = (hash * 1097) + c;
        return hash;
    }
};

template <size_t N>
consteval StringLiteral<N + 1> placeholder() {
    char buf[N + 1] = {0};
    std::fill_n(buf, N, ' ');
    buf[N] = 0;
    return {buf};
}

template <size_t N>
consteval StringLiteral<naive_digits10(N) + 1> unsigned_literal() {
    char buf[naive_digits10(N) + 1] = {0};
    std::to_chars(buf, buf + sizeof(buf) - 1, N);
    buf[sizeof(buf) - 1] = 0;
    return {buf};
}

template <ssize_t N>
consteval StringLiteral<naive_digits10(N) + 2> signed_literal() {
    char buf[naive_digits10(N) + 2] = {0};
    buf[sizeof(buf) - 2] = ' ';
    std::to_chars(buf, buf + sizeof(buf) - 1, N);
    buf[sizeof(buf) - 1] = 0;
    return {buf};
}

template <class T>
concept CCharRange = std::ranges::range<T> &&
                     std::is_same_v<std::ranges::range_value_t<T>, char>;

template <class T>
concept CCharIter =
    std::input_iterator<T> && std::is_same_v<std::iter_value_t<T>, char>;

template <class F>
concept CMaybeFiller =
    std::same_as<F, std::nullptr_t> || requires(F f, char *chars) {
        { std::move(f)(chars) } -> std::same_as<void>;
    };

// TODO: define a specialization for when TCharRange has a comptime-known size
template <StringLiteral KBlank>
class BlankBuffer {
    char *_data;

   public:
    // TODO: should only provide this to formatters
    constexpr auto data() { return _data; }

    template <size_t KSize>
    constexpr BlankBuffer(std::array<char, KSize> &a) : _data(a.data()) {
        static_assert(KSize >= KBlank.size(), "array too small");
        std::copy_n(KBlank.data(), KBlank.size(), _data);
    }
};

template <size_t KSize>
class SentinelBuffer {
    size_t sentinel = 0;
    std::array<char, KSize> _data;

   public:
    // TODO: should only provide this to formatters
    template <StringLiteral KBlank = "">
    constexpr auto data() {
        static_assert(KBlank.size() <= KSize, "buffer too small");
        if (sentinel != KBlank.hash()) [[unlikely]]
            std::copy_n(KBlank.data(), KBlank.size(), _data.begin());
        return _data.data();
    }
};

template <StringLiteral KBlank>
class ConstInitBlankBuffer {
    std::array<char, KBlank.size()> _data;

   public:
    // TODO: should only provide this to formatters
    constexpr auto data() { return _data.data(); }

    constexpr ConstInitBlankBuffer() {
        std::copy_n(KBlank.data(), KBlank.size(), _data.begin());
    }
};

template <size_t KOffset, CMaybeFiller TFiller>
struct OffsetFiller {
    TFiller filler;

    constexpr void operator()(char *chars) && {
        if constexpr (!std::is_same_v<std::nullptr_t, TFiller>)
            std::move(filler)(chars + KOffset);
    }
};

template <size_t KOffset, CMaybeFiller TFirst, CMaybeFiller TSecond>
struct ConcatFiller {
    TFirst first;
    TSecond second;

    constexpr void operator()(char *chars) && {
        if constexpr (!std::is_same_v<std::nullptr_t, TFirst>)
            std::move(first)(chars);
        if constexpr (!std::is_same_v<std::nullptr_t, TSecond>)
            std::move(second)(chars + KOffset);
    }
};

template <std::integral T>
consteval size_t integer_placeholder_length() {
    return naive_digits10(std::numeric_limits<T>::max()) +
           (std::is_signed_v<T> ? 1 : 0);
}

template <StringLiteral KBlank = "", CMaybeFiller TFiller = std::nullptr_t>
struct Form {
    consteval static size_t length() { return KBlank.size() - 1; }
    TFiller filler;

    // TODO: only allow for other instantiations
    explicit constexpr Form(TFiller &&filler) : filler(filler) {}

    template <CMaybeFiller TOtherFiller>
    constexpr Form<KBlank, TOtherFiller> with_filler(
        TOtherFiller &&other_filler) const {
        using TNewForm = Form<KBlank, TOtherFiller>;
        return TNewForm{std::move(other_filler)};
    }

    using ConstInitBuffer = ConstInitBlankBuffer<KBlank>;
    using Buffer = BlankBuffer<KBlank>;

    constexpr void write_to(ConstInitBuffer &buf) && {
        if constexpr (!std::is_same_v<std::nullptr_t, TFiller>)
            std::move(filler)(buf.data());
    }

    constexpr void write_to(Buffer buf) && {
        if constexpr (!std::is_same_v<std::nullptr_t, TFiller>)
            std::move(filler)(buf.data());
    }

    template <size_t KSize>
    constexpr void write_to(SentinelBuffer<KSize> &buf) && {
        if constexpr (!std::is_same_v<std::nullptr_t, TFiller>)
            std::move(filler)(buf.template data<KBlank>());
    }
};

template <StringLiteral KLeftBlank, CMaybeFiller TLeftFiller,
          StringLiteral KRightBlank, CMaybeFiller TRightFiller>
constexpr auto operator+(Form<KLeftBlank, TLeftFiller> &&left,
                         Form<KRightBlank, TRightFiller> &&right) {
    using TConcatFiller =
        ConcatFiller<KLeftBlank.size() - 1, TLeftFiller, TRightFiller>;
    using TNewForm = Form<KLeftBlank + KRightBlank, TConcatFiller>;
    auto new_filler =
        TConcatFiller{std::move(left.filler), std::move(right.filler)};
    return TNewForm{std::move(new_filler)};
}

template <StringLiteral KLeftBlank, CMaybeFiller TLeftFiller,
          StringLiteral KRightBlank>
constexpr auto operator+(Form<KLeftBlank, TLeftFiller> &&left,
                         const Form<KRightBlank, std::nullptr_t> &right) {
    using TNewForm = Form<KLeftBlank + KRightBlank, TLeftFiller>;
    return TNewForm{std::move(left.filler)};
}

template <StringLiteral KLeftBlank, StringLiteral KRightBlank,
          CMaybeFiller TRightFiller>
constexpr auto operator+(const Form<KLeftBlank, std::nullptr_t> &left,
                         Form<KRightBlank, TRightFiller> &&right) {
    using TOffsetFiller = OffsetFiller<KLeftBlank.size() - 1, TRightFiller>;
    auto new_filler = TOffsetFiller{std::move(right.filler)};
    using TNewForm = Form<KLeftBlank + KRightBlank, TOffsetFiller>;
    return TNewForm{std::move(new_filler)};
}

template <StringLiteral KLeftBlank, StringLiteral KRightBlank>
constexpr auto operator+(const Form<KLeftBlank, std::nullptr_t> &left,
                         const Form<KRightBlank, std::nullptr_t> &right) {
    using TNewForm = Form<KLeftBlank + KRightBlank, std::nullptr_t>;
    return TNewForm{nullptr};
}

template <CMaybeFiller TFiller>
Form(TFiller &&) -> Form<"", TFiller>;

namespace forms {

template <StringLiteral KLiteral>
constexpr auto k = Form<KLiteral>{nullptr};

template <size_t KValue>
constexpr auto ku = Form<unsigned_literal<KValue>()>{nullptr};

template <ssize_t KValue>
constexpr auto ki = Form<signed_literal<KValue>()>{nullptr};

template <StringLiteral KLiteral>
constexpr auto qk = k<"\""> + k<KLiteral> + k<"\"">;

constexpr auto v(bool value) {
    return k<placeholder<5>()>.with_filler([value](char *chars) {
        std::copy_n(value ? " true" : "false", 5, chars);
    });
}

template <std::integral T>
constexpr auto v(T value) {
    constexpr auto length = integer_placeholder_length<T>();
    return k<placeholder<length>()>.with_filler([value](char *chars) {
        std::fill_n(chars, length, ' ');
        std::to_chars(chars, chars + length, value);
    });
}

template <size_t KLength, CCharIter TCharIter>
constexpr auto v(TCharIter value) {
    return k<placeholder<KLength>()>.with_filler([value](char *chars) {
        std::fill_n(chars, KLength, ' ');
        std::copy_n(value, KLength, chars);
    });
}

template <class T>
constexpr auto qv(T &&value) {
    return k<"\""> + v(std::move(value)) + k<"\"">;
}

template <size_t KLength, class T>
constexpr auto qv(T &&value) {
    return k<"\""> + v<KLength>(std::move(value)) + k<"\"">;
}

}  // namespace forms

}  // namespace matter

// --- 8< ---

#include <cstdio>

constexpr inline auto stanza_form(uint32_t count) {
    using namespace matter::forms;
    // clang-format off
    return v(count) + k<" bottles of beer on the wall,\n">
        + v(count) + k<" bottles of beer.\n">
        + k<"Take one down, pass it around,\n">
        + v(count - 1) + k<" bottles of beer on the wall.\n">;
    // clang-format on
}

constexpr inline auto stanza_json_form(uint32_t count) {
    using namespace matter::forms;
    // clang-format off
    return k<R"({"count":)"> + v(count)
        + k<R"(,"text":")"> + stanza_form(count)
        + k<"\"}">;
    // clang-format on
}

template <matter::StringLiteral KStatus, class TBodyForm>
constexpr inline auto http_response(TBodyForm &&body_form) {
    using namespace matter::forms;

    // clang-format off
    return k<"HTTP/1.1 "> + k<KStatus>
        + k<"\r\nContent-Length:"> + ku<body_form.length()>
        + k<"\r\nContent-Type:"> + k<"application/json">
        + k<"\r\n\r\n"> + std::move(body_form);
    // clang-format on
}

int main(int argc, const char **argv) {
    uint32_t max_count = 0xFFFFFF;
    if (argc > 1) max_count = std::strtoul(argv[1], 0, 10);

    // constexpr static auto empty_form =
    //     http_response<"200 OK">(stanza_json_form({}));
    // static decltype(empty_form)::ConstInitBuffer buf;

    matter::SentinelBuffer<256> buf;

    // std::array<char, 256> buf;

    for (uint32_t count = max_count; count > 0; count -= 1) {
        http_response<"200 OK">(stanza_json_form(count)).write_to(buf);
        std::puts(buf.data());
    }
}
