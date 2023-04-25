#! python3

import exrex
import os

def ToplevelFolder():
    scriptPath = os.path.realpath(__file__)
    parentDirectory = os.path.dirname(scriptPath)
    return os.path.join(parentDirectory, "py")

def EnumDeclarationsFolder():
    return os.path.join(ToplevelFolder(), "dcl.enum")

def EmptyDeclarationFolder():
    return os.path.join(ToplevelFolder(), "dcl.dcl", "empty_declaration")


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


def GenerateIndexedCppFiles(parentDirectory, fileContents):
    os.makedirs(parentDirectory, exist_ok=True)
    for index, aDeclaration in enumerate(fileContents):
        fileName = str(index) + ".cpp"
        filePath = os.path.join(parentDirectory, fileName)
        with open(filePath, "w") as f:
            f.write(aDeclaration)


GenerateIndexedCppFiles(EnumDeclarationsFolder(), GenerateEnumDeclarations())
#https://eel.is/c++draft/dcl.dcl#nt:empty-declaration
GenerateIndexedCppFiles(EmptyDeclarationFolder(), [";"])
