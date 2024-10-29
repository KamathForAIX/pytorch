#pragma once

#include <c10/util/ConstexprCrc.h>
#include <c10/util/IdWrapper.h>
#include <c10/util/string_view.h>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>

namespace c10::util {

struct type_index final : IdWrapper<type_index, uint64_t> {
  constexpr explicit type_index(uint64_t checksum) : IdWrapper(checksum) {}

  // Allow usage in std::map / std::set
  // TODO Disallow this and rather use std::unordered_map/set everywhere
  friend constexpr bool operator<(type_index lhs, type_index rhs) noexcept {
    return lhs.underlyingId() < rhs.underlyingId();
  }

  friend std::ostream& operator<<(std::ostream& stream, type_index typeId) {
    return stream << typeId.underlyingId();
  }
};

namespace detail {

template <typename T>
inline constexpr std::string_view fully_qualified_type_name_impl() {
#if defined(_MSC_VER) && !defined(__clang__)
  constexpr std::string_view str = __FUNCSIG__;
#if defined(__NVCC__)
  constexpr std::string_view prefix =
      "std::basic_string_view<char> c10::util::detail::fully_qualified_type_name_impl<";
  constexpr std::string_view suffix = ">()";
#else
  constexpr std::string_view prefix =
      "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl c10::util::detail::fully_qualified_type_name_impl<";
  constexpr std::string_view suffix = ">(void)";
#endif
#else
  constexpr std::string_view str = __PRETTY_FUNCTION__;
#if defined(__clang__)
  constexpr std::string_view prefix =
      "std::string_view c10::util::detail::fully_qualified_type_name_impl() [T = ";
  constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
  constexpr std::string_view prefix =
      "constexpr std::string_view c10::util::detail::fully_qualified_type_name_impl() [with T = ";
  constexpr std::string_view suffix =
      "; std::string_view = std::basic_string_view<char>]";
#endif
#endif
  static_assert(c10::string_view_starts_with(str, prefix));
  static_assert(c10::string_view_ends_with(str, suffix));
  return str.substr(prefix.size(), str.size() - prefix.size() - suffix.size());
}

#if !defined(__CUDA_ARCH__)
template <typename T>
inline constexpr uint64_t type_index_impl() {
// Idea: __PRETTY_FUNCTION__ (or __FUNCSIG__ on msvc) contains a qualified name
// of this function, including its template parameter, i.e. including the
// type we want an id for. We use this name and run crc64 on it to get a type
// id.
#if defined(_MSC_VER) && !defined(__clang__)
  return crc64(__FUNCSIG__, sizeof(__FUNCSIG__)).checksum();
#elif defined(__clang__)
  return crc64(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__)).checksum();
#elif defined(__GNUC__)
  return crc64(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__)).checksum();
#endif
}
#endif

} // namespace detail

template <typename T>
inline constexpr type_index get_type_index() {
#if !defined(__CUDA_ARCH__)
  // To enforce that this is really computed at compile time, we pass the
  // type index through std::integral_constant.
  return type_index{std::integral_constant<
      uint64_t,
      detail::type_index_impl<std::decay_t<T>>()>::value};
#else
  // There's nothing in theory preventing us from running this on device code
  // except for nvcc throwing a compiler error if we enable it.
  return (abort(), type_index(0));
#endif
}

#if !defined(TORCH_PEDANTIC)
// Use precomputed hashsum for std::string
// Needed to workaround ambiguity in class name resolution
// into __PRETTY_FUNCTION__ when abovementioned class is defined in inlined
// namespace. In multi-ABI C++ library, `std::string` is an alias to
// `std::__cxx11::basic_string<char>` which depending on compiler flags can be
// resolved to `basic_string<char>` either in `std` namespace or in
// `std::__cxx11` one (`__cxx11` is an inline namespace)
template <>
inline constexpr type_index get_type_index<std::string>() {
  // hashsum for std::basic_string<char>
  return type_index{4193213214807308375ULL};
}
#endif

template <typename T>
inline constexpr std::string_view get_fully_qualified_type_name() noexcept {
  return detail::fully_qualified_type_name_impl<T>();
}
} // namespace c10::util

C10_DEFINE_HASH_FOR_IDWRAPPER(c10::util::type_index)
