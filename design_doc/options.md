# ELF Option Parser and Interface

## 0. Design of Option
 1. Basically in the folder src_cpp/elf/options/ and src_py/elf/options/;
 2. Important classes like **game**, **model**, **trainer**, **evaluator**, **sampler** and so forth;

## 1. Option Spec Class
 1. All arguments from command line are parsed by argparser in py_option_spec.py
 2. class **PyOptionSpec** does the parsing;
 3. Inheritted from class **OptionSpec** in C++;
 3. After doing the parsing, **parse()** will convert the parsed arguments to a Option Map class.


## 2. Option Map Class
 1. A class to handle Argument by Json shared info between python and C++;
 2. class **PyOptionMap** receives a **PyOptionSpec** class and saves as a member;
 3. Inherited from class **OptionMap** in C++;

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
