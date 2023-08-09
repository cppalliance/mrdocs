#
# Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

import gdb


class utils:
    @staticmethod
    def resolve_type(t):
        if t.code == gdb.TYPE_CODE_REF:
            t = t.target()
        t = t.unqualified().strip_typedefs()
        typename = t.tag
        if typename is None:
            return None
        return t

    @staticmethod
    def resolved_typename(val):
        t = val.type
        t = utils.resolve_type(t)
        if t is not None:
            return str(t)
        else:
            return str(val.type)

    @staticmethod
    def pct_decode(s: str):
        return urllib.parse.unquote(s)

    sv_pool = []

    @staticmethod
    def make_string_view(cstr: gdb.Value, n: int):
        sv_ptr: gdb.Value = gdb.parse_and_eval('malloc(sizeof(class boost::core::basic_string_view<char>))')
        sv_ptr_str = cstr.format_string(format='x')
        gdb.execute(
            f'call ((boost::core::basic_string_view<char>*){sv_ptr})->basic_string_view((const char*){sv_ptr_str}, {n})',
            to_string=True)
        sv = gdb.parse_and_eval(f'*((boost::core::basic_string_view<char>*){sv_ptr})')
        copy: gdb.Value = sv
        utils.sv_pool.append(sv_ptr)
        if len(utils.sv_pool) > 5000:
            gdb.execute(f'call free({utils.sv_pool[0]})', to_string=True)
            utils.sv_pool.pop(0)
        return copy

    pct_sv_pool = []

    @staticmethod
    def make_pct_string_view(cstr: gdb.Value, n: int):
        sv_ptr: gdb.Value = gdb.parse_and_eval('malloc(sizeof(class boost::urls::pct_string_view))')
        sv_ptr_str = cstr.format_string(format='x')
        gdb.execute(
            f'call ((boost::urls::pct_string_view*){sv_ptr})->pct_string_view((const char*){sv_ptr_str}, {n})',
            to_string=True)
        sv = gdb.parse_and_eval(f'*((boost::urls::pct_string_view*){sv_ptr})')
        copy: gdb.Value = sv
        utils.pct_sv_pool.append(sv_ptr)
        if len(utils.sv_pool) > 5000:
            gdb.execute(f'call free({utils.pct_sv_pool[0]})', to_string=True)
            utils.sv_pool.pop(0)
        return copy


class DomValuePrinter:
    def __init__(self, value):
        self.value = value

    def children(self):
        # Get kind enum
        kind = self.value['kind_']
        if kind == 1:
            yield 'Boolean', self.value['b_']
        elif kind == 2:
            yield 'Integer', self.value['i_']
        elif kind == 3:
            yield 'String', self.value['str_']
        elif kind == 4:
            yield 'Array', self.value['arr_']
        elif kind == 5:
            yield 'Object', self.value['obj_']['impl_']['_M_ptr']
        elif kind == 6:
            yield 'Function', self.value['fn_']
        else:
            yield 'kind_', self.value['kind_']

class DomStringPrinter:
    def __init__(self, value):
        self.value = value

    def to_string(self):
        return self.value['psz_']


class EnumPrinter:
    def __init__(self, value):
        self.value = value

    def to_string(self):
        s: str = self.value.format_string(raw=True)
        return s.rsplit(':', 1)[-1]


if __name__ != "__main__":
    def lookup_function(val: gdb.Value):
        if val.type.code == gdb.TYPE_CODE_ENUM:
            return EnumPrinter(val)

        typename: str = utils.resolved_typename(val)
        if typename == 'clang::mrdox::dom::Value':
            return DomValuePrinter(val)
        if typename == 'clang::mrdox::dom::String':
            return DomStringPrinter(val)
        return None


def register_mrdox_printers(obj=None):
    if obj is None:
        obj = gdb
    obj.pretty_printers.append(lookup_function)
