#! python3

import exrex
import os
import sys
import math

# You can call this script with a target directory as command line argument.
# If not, we fallback to the directory of this script.
# Within this target diretory, we always add a subdiretory 'py'.
def ParseTargetDirectory():
    if len(sys.argv) > 1:
        return sys.argv[1]
    else:
        return os.path.join(os.path.dirname(os.path.realpath(__file__)), "py")

providedTargetDirectory = ParseTargetDirectory()

def EnumDeclarationsFolder():
    return os.path.join(providedTargetDirectory, "dcl.enum")

def EmptyDeclarationFolder():
    return os.path.join(providedTargetDirectory, "dcl.dcl", "empty_declaration")

def NamespaceDefinitionsFolder():
    return os.path.join(providedTargetDirectory, "namespace.def")

def ClassSpecifiersFolder():
    return os.path.join(providedTargetDirectory, "class.pre")

def NonMemberFunctionDeclarationsFolder():
    return os.path.join(providedTargetDirectory, "functions", "non-member")


def GenerateEnumDeclarations():
    #https://eel.is/c++draft/enum
    enum_base = "(|: short|: unsigned int|: const long|: const volatile long long|: decltype\(0\))"
    enumerator_initializer = "(| = 0| = true \? 1,2 : 3| = \!\+\[\]\(\)\{\})"
    enumerators = " (|(A" + enumerator_initializer + "|A" + enumerator_initializer + ", B " + enumerator_initializer + "),?) "
    regex = "enum (|EnumName|class EnumName|struct EnumName) " + enum_base + " (\{" + enumerators + "\})? ;"
    generator = exrex.generate(regex)
    declarations = list(generator)
    # Unfortunately, the regex produces some invalid declarations. We remove them by hand as of now.
    for invalidDeclaration in ["enum    ;", "enum  : short  ;", "enum  : unsigned int  ;",
                               "enum  : const long  ;", "enum  : const volatile long long  ;",
                               "enum  : decltype(0)  ;", "enum EnumName   ;"]:
        try:
            declarations.remove(invalidDeclaration)
        except ValueError:
            print("The invalid declaration \"" + invalidDeclaration + "\" gets no longer generated, so it can be removed from the list of invalid declarations.")
    return declarations


def GenerateNamespaceDefinitions():
    #https://eel.is/c++draft/namespace.def
    optional_content = "(class SampleClass \{\}; int SampleObject = 0; template<class T, auto V, template<class> class Templ> class SampleClassTemplate \{\};)?"
    named_namespace_definition = f"(inline )?namespace ParseMeIfYouCan \{{ {optional_content} \}}"
    unnamed_namespace_definition = f"(inline )?namespace \{{ {optional_content} \}}"
    nested_namespace_definition = f"namespace Level_2::( inline )?Level_1::( inline )?Level_0 \{{ {optional_content} \}}"
    regex = f"({named_namespace_definition}|{unnamed_namespace_definition}|{nested_namespace_definition})"
    generator = exrex.generate(regex)
    return generator


def GenerateClassSpecifiers():
    #https://eel.is/c++draft/class.pre
    base_classes = "struct base_1\{\};\nclass base_2\{\};\n"
    class_key = "(class|struct)"
    class_name = "(C|C final)"
    access_specifier = "(|public|private|protected)"
    class_or_decltype_1 = f"(base_1|decltype\(base_1\{{\}}\))"
    class_or_decltype_2 = f"(base_2|decltype\(base_2\{{\}}\))"
    base_specifier_1 = f"({access_specifier} {class_or_decltype_1}|virtual {access_specifier} {class_or_decltype_1}|{access_specifier} virtual {class_or_decltype_1})"
    base_specifier_2 = f"({access_specifier} {class_or_decltype_2}|virtual {access_specifier} {class_or_decltype_2}|{access_specifier} virtual {class_or_decltype_2})"
    base_clause = f"(|: {base_specifier_1}|: {base_specifier_1}, {base_specifier_2})"
    class_or_struct = f"{class_key} ({class_name} {base_clause} \{{\}};| {base_clause} \{{\}} obj;)"
    union = "(union U \{\};)|(union \{\} obj;)"
    regex = f"({base_classes}{class_or_struct})|({union})"
    generator = exrex.generate(regex)
    return generator


def GenerateNonMemberFunctions():
    special_cases = """
int const f\(\);|
int volatile f\(\);|
int const volatile f\(\);|
int\* f\(\);|
int& f\(\);|
int&& f\(\);|
void f\(bool const b\);|
void f\(bool volatile b\);|
void f\(bool const volatile b\);|
void f\(bool& b\);|
void f\(bool&& b\);|
int f\(bool b\.\.\.\);|
void f\(\) noexcept;|
void f\(\) noexcept\(false\);|
extern "C" int f\(bool b\);|
namespace CppScope \{ extern "C" int f\(bool b\); \}|
extern "C" \{ extern "C\+\+" int f\(bool b\); \}|
int f\(bool b\) = delete;|
int f\(bool b\) try \{ return b \? 1 : 2; \} catch\(\.\.\.\) \{ throw; \}|
int f\(bool b\) noexcept\(false\) try \{ return b \? 1 : 2; \} catch\(\.\.\.\) \{ throw; \}|
auto f\(bool b\) -> int;|
auto f\(bool b\) -> auto \{ return b; \}|
class C;\nint C::\* f\(int C::\* p\);|
class C;\nusing MemFnPtr = char\(C::\*\)\(int\) const&;\nconst MemFnPtr& f\(MemFnPtr a, MemFnPtr& b, MemFnPtr&& c, const MemFnPtr& d\);
"""
    all_fundamental_types_except_void = "decltype\(nullptr\)|bool|char|signed char|unsigned char|char8_t|char16_t|char32_t|wchar_t|short|int|long|long long|unsigned short|unsigned int|unsigned long|unsigned long long|float|double|long double"
    all_fundamental_types = f"{all_fundamental_types_except_void}|void"
    function_returning_fundamental_types = f"({all_fundamental_types}) f\(\);"
    function_taking_one_fundamental_type = f"void f\(({all_fundamental_types_except_void}) arg\);"
    regex = f"({function_returning_fundamental_types}|{function_taking_one_fundamental_type}|{special_cases})"
    generator = exrex.generate(regex)
    return generator

def GenerateIndexedCppFiles(parentDirectory, fileContents):
    os.makedirs(parentDirectory, exist_ok=True)
    contents = list(fileContents)
    max_len = int(math.log10(len(contents))) + 1
    name_fmt = f"{{:->{max_len}}}.cpp"

    for index, aDeclaration in enumerate(contents):
        fileName = name_fmt.format(index)
        filePath = os.path.join(parentDirectory, fileName)
        with open(filePath, "w") as f:
            f.write(aDeclaration)



GenerateIndexedCppFiles(EnumDeclarationsFolder(), GenerateEnumDeclarations())
#https://eel.is/c++draft/dcl.dcl#nt:empty-declaration
GenerateIndexedCppFiles(EmptyDeclarationFolder(), [";"])
GenerateIndexedCppFiles(NamespaceDefinitionsFolder(), GenerateNamespaceDefinitions())
GenerateIndexedCppFiles(ClassSpecifiersFolder(), GenerateClassSpecifiers())
GenerateIndexedCppFiles(NonMemberFunctionDeclarationsFolder(), GenerateNonMemberFunctions())
