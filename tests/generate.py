import exrex
import os

def ToplevelFolder():
    scriptPath = os.path.realpath(__file__)
    parentDirectory = os.path.dirname(scriptPath)
    return os.path.join(parentDirectory, "py")

def EnumDeclarationsFolder():
    return os.path.join(ToplevelFolder(), "dcl.enum")


def GenerateEnumDeclarations():
    #https://eel.is/c++draft/enum
    enum_base = "(|: short|: unsigned int|: const long|: const volatile long long|: decltype\(0\))"
    enumerator_initializer = "(| = 0| = true \? 1,2 : 3)"
    enumerators = " (|(A" + enumerator_initializer + "|A" + enumerator_initializer + ", B " + enumerator_initializer + "),?) "
    regex = "enum (|EnumName|class EnumName|struct EnumName) " + enum_base + " (\{" + enumerators + "\})? ;"
    generator = exrex.generate(regex)
    first = next(generator) #The first entry is "enum    ;", which is not valid. Therefore, we skip it.
    if first != "enum    ;":
        raise Exception("We are expecting that the first value is 'enum    ;', which should be skipped. But our expectation is not met. The first value is '" + first + "'.")
    return generator


def GenerateIndexedCppFiles(parentDirectory, fileContents):
    os.makedirs(parentDirectory, exist_ok=True)
    for index, aDeclaration in enumerate(fileContents):
        fileName = str(index) + ".cpp"
        filePath = os.path.join(parentDirectory, fileName)
        with open(filePath, "w") as f:
            f.write(aDeclaration)


GenerateIndexedCppFiles(EnumDeclarationsFolder(), GenerateEnumDeclarations())
