#pragma once

#include "member_check.h"

/* Example usage
 *
class Visitor {
 public:
  template <typename T>
  void visit(const std::string &name, const T& v, const std::string& help) const
{
    std::cout << name << ", " << v << ", help: " << help << std::endl;
  }
};

DEF_STRUCT(Options) 
  DEF_FIELD(int, id, 0, "The id");
  DEF_FIELD(std::string, name, "", "The name");
  DEF_FIELD(float, salary, 0.0, "The salary");
DEF_END

int main() {
  Options options;
  Visitor visitor;

  options.id = 2;
  options.name = "Smith";
  options.salary = 2.5;

  options.apply(visitor);
  return 0;
}
 *
 */

template <int I>
class Idx {
 public:
  static constexpr int value = I;
  using prev_type = Idx<I - 1>;
  using next_type = Idx<I + 1>;
};

template <typename C, typename I, int line_number>
static constexpr int __NextCounter() {
  if constexpr (std::is_same<decltype(C::__identifier(I{})), int>::value) {
    return I::value;
  } else {
    return __NextCounter<C, Idx<I::value + 1>, line_number>();
  }
}

#define _INIT(class_name)                      \
  using __curr_type = class_name;              \
  static constexpr bool __reflection = true;   \
  template <typename I>                        \
  static constexpr int __identifier(I);        \
  static constexpr bool __identifier(Idx<0>);  \
  template <typename Visit>                    \
  void __apply(const Visit&, Idx<0>) const {}  \
  template <typename Visit>                    \
  void __applyMutable(const Visit&, Idx<0>) {} \
  template <typename Visit>                    \
  static void __applyStatic(const Visit&, Idx<0>) {}

#define DEF_STRUCT(class_name) \
  struct class_name {          \
    _INIT(class_name)

#define _MEMBER_FUNCS(name, help, idx_name)           \
  static constexpr int idx_name =                     \
      __NextCounter<__curr_type, Idx<0>, __LINE__>(); \
  static constexpr bool __identifier(Idx<idx_name>);  \
  template <typename Visit>                           \
  void __apply(Visit& v, Idx<idx_name>) const {       \
    v.template visit(#name, name, help);              \
    __apply(v, Idx<idx_name - 1>{});                  \
  }                                                   \
  template <typename Visit>                           \
  void __applyMutable(Visit& v, Idx<idx_name>) {      \
    v.template visit(#name, name, help);              \
    __applyMutable(v, Idx<idx_name - 1>{});           \
  }

#define _STATIC_FUNCS(name, def, help, idx_name)       \
  template <typename Visit>                            \
  static void __applyStatic(Visit& v, Idx<idx_name>) { \
    v.template visit(#name, def, help);                \
    __applyStatic(v, Idx<idx_name - 1>{});             \
  }

#define _STATIC_FUNCS_NODEFAULT(type, name, help, idx_name) \
  template <typename Visit>                                 \
  static void __applyStatic(Visit& v, Idx<idx_name>) {      \
    v.template visit(#name, type(), help);                  \
    __applyStatic(v, Idx<idx_name - 1>{});                  \
  }

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)

#define DEF_FIELD(type, name, def, help) \
  DEF_FIELD_IMPL(type, name, def, help, CONCAT(__idx, name))

#define DEF_FIELD_IMPL(type, name, def, help, idx_name) \
  type name = def;                                      \
  _MEMBER_FUNCS(name, help, idx_name)                   \
  _STATIC_FUNCS(name, def, help, idx_name)

#define DEF_FIELD_NODEFAULT(type, name, help) \
  DEF_FIELD_NODEFAULT_IMPL(type, name, help, CONCAT(__idx, name))

#define DEF_FIELD_NODEFAULT_IMPL(type, name, help, idx_name) \
  type name;                                                 \
  _MEMBER_FUNCS(name, help, idx_name)                        \
  _STATIC_FUNCS_NODEFAULT(type, name, help, idx_name)

#define DEF_END                                       \
  static constexpr const int __final_idx =            \
      __NextCounter<__curr_type, Idx<0>, __LINE__>(); \
  template <typename Visit>                           \
  void apply(Visit& v) const {                        \
    __apply(v, Idx<__final_idx - 1>{});               \
  }                                                   \
  template <typename Visit>                           \
  void applyMutable(Visit& v) {                       \
    __applyMutable(v, Idx<__final_idx - 1>{});        \
  }                                                   \
  template <typename Visit>                           \
  static void applyStatic(Visit& v) {                 \
    __applyStatic(v, Idx<__final_idx - 1>{});         \
  }                                                   \
  }                                                   \
  ;

namespace elf {
namespace reflection {

MEMBER_CHECK(__reflection)

template <typename T>
using has_reflection = has___reflection<T>;

} // namespace reflection
} // namespace elf
