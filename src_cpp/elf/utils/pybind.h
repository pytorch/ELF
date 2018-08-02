#pragma once

#define PB_INIT(C)         \
  {                        \
    using __curr_type = C; \
    py::class_<C>(m, #C)

#define PB_FIELD(field) .def_readwrite(#field, &__curr_type::field)

#define PB_END \
  ;            \
  }
