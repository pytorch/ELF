# ELF Option Parser and Interface

Since ELF Go project has a lot of options, we have a separate folding to manage and implement options in both C++ and python.

## Design of Option
- Basically in the folder src_cpp/elf/options/ and src_py/elf/options/;
- Important classes like **game**, **model**, **trainer**, **evaluator**, **sampler** and so forth;

## Option Spec Class (Python)
- All arguments from command line are parsed by argparser in py_option_spec.py
- class **PyOptionSpec** does the parsing;
- Inheritted from class **OptionSpec** in C++;
- After doing the parsing, **parse()** will convert the parsed arguments to a Option Map class.

## Option Map Class (Python)
- A class to handle Argument by Json shared info between python and C++;
- class **PyOptionMap** receives a **PyOptionSpec** class and saves as a member;
- Inherited from class **OptionMap** in C++;

## 3. Detailed Design
- class OptionSpec (C++)
```cpp
class OptionSpec {
public:
 bool addOption(string optionName, string help)
 bool addOption(string optionName, string help, T defaultValue)
 vector<string> getOptionNames(): get all option names as a vector;
 json **getPythonArgparseOptionsAsJSON()** wrap everything up in a json array;
 string getPythonArgparseOptionsAsJSONString()
 addPrefixSuffixToOptionNames(string prefix, string suffix)
 
private:
 const OptionBase& getOptionInfo()
 unordered_map<string, shared_ptr<OptionBase>> optionSpecMap_;
}
```
