#pragma once

#define PB_INIT(C)         \
  {                        \
    using __curr_type = C; \
    m.def("saveDefault" #C "ToArgs", [](const std::string& prefix, elf::options::OptionSpec& spec) { elf::options::Visitor<__curr_type> visitor(prefix, spec); }); \
    py::class_<C>(m, #C)

#define PB_FIELD(field) .def_readwrite(#field, &__curr_type::field)

#define PB_END \
  .def("saveToArgs", [](const __curr_type& obj, const std::string& prefix, elf::options::OptionSpec& spec) { elf::options::Loader<__curr_type> visitor(prefix, spec, obj); }) \
  .def("loadFromArgs", [](__curr_type& obj, const std::string& prefix, const elf::options::OptionSpec& spec) { elf::options::Saver<__curr_type> visitor(prefix, spec, obj); }) \
  ; \
  }
