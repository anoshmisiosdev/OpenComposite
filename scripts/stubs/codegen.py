import re
from typing import List, Optional

from stubs.interface import InterfaceDef, Function
from stubs.interface_spec import InterfaceSpec

cflag_spec = re.compile(r"\[(?P<name>\w+)\]\s*=\s*(?P<value>.*)")

# ---------------------------------------------------------------------------
# x64 vtable ABI mismatch fix
# ---------------------------------------------------------------------------
#
# Background: a non-static C++ member function that returns a struct/union too
# large to fit in RAX (i.e. anything that isn't 1/2/4/8 bytes) must return it
# via a caller-allocated "hidden pointer". MSVC's x64 ABI keeps `this` in RCX
# and inserts that hidden pointer as an implicit *second* argument in RDX for
# this specific case. GCC/mingw-w64's ABI instead always treats the hidden
# pointer as the true first argument (as it would for a free function),
# pushing `this` to RDX. This has been independently verified (see the
# vtable-abi-fix branch's commit messages for the full writeup): against
# Microsoft's own docs plus a detailed third-party ABI writeup for the MSVC
# side, and empirically via objdump against the real x86_64-w64-mingw32-g++
# for the GCC side, and reproduced end-to-end with a hand-written asm harness
# that calls a real mingw-compiled vtable slot using each convention.
#
# Every OpenVR interface method exposed on our raw C++ vtable is a non-static
# member function. Any of them whose return type is one of the structs below
# is therefore vulnerable: a real MSVC-compiled game calling directly through
# the vtable (as opposed to through the `FnTable:`-style flat C function
# pointer table, which never goes through the C++ ABI at all and needs no fix)
# will present `this`/hidden-pointer in the opposite registers to what our
# GCC-compiled body expects, so the struct write lands on top of the
# interface object itself, corrupting it - typically clobbering its vtable
# pointer with a stray float bit-pattern from the struct that should have
# been returned, and crashing on the next virtual call.
#
# The fix is a tiny naked (pure asm, no compiler-generated prologue/epilogue)
# trampoline placed in the vtable slot itself: it swaps RCX/RDX (the only two
# registers that ever hold `this`/hidden-pointer - real arguments always start
# at R8 in both conventions, so nothing else needs touching) and tail-jumps
# (not calls, so the real implementation's own RAX-pointer return behaviour is
# preserved) into the real GCC-ABI implementation. That real implementation is
# generated as a separate extern "C" free function (not the override itself),
# so that OpenComposite's own GCC-compiled code - namely the FnTable stub
# functions below, the only internal code that ever calls through one of
# these methods - can keep calling it directly with the normal, self
# consistent GCC ABI, without ever going anywhere near the swapped trampoline.
#
# This table is the complete, closed set of OpenVR struct/union types ever
# returned by value from an interface method, across every header revision
# vendored in OpenVRHeaders/ (openvr-0.9.12.h through openvr-2.5.1.h, plus the
# custom_interfaces/*.h) - confirmed by grepping every `virtual <type> name(`
# declaration in all of them. Each one has been byte-identical since
# openvr-0.9.12, and a static_assert is emitted next to every use so that a
# future header revision that changes a layout (or introduces a new
# unlisted large-struct return) fails the build loudly rather than silently
# reintroducing the corruption.
LARGE_STRUCT_RETURN_TYPES = {
    "HmdMatrix34_t": 48,
    "HmdMatrix44_t": 64,
    "DistortionCoordinates_t": 24,
    "HiddenAreaMesh_t": 16,
    "HmdColor_t": 16,
}


def _bare_return_type_name(return_type: str) -> Optional[str]:
    """
    Reduce a possibly namespace-qualified, possibly-const return type string down to
    its bare type name, or return None if it's a pointer/reference type. Only a
    genuine by-value struct/union return can ever trigger the hidden-return-pointer
    ABI special case, so pointer/reference returns are never affected.
    """
    t = return_type.strip()
    if t.endswith("*") or t.endswith("&"):
        return None
    if t.startswith("const "):
        t = t[len("const "):].strip()
    return t.rsplit("::", 1)[-1]


def needs_abi_trampoline(return_type: str) -> bool:
    """
    True if a non-static member function returning this type by value must be
    placed behind the RCX/RDX register-swap trampoline to be safely callable
    through a raw C++ vtable by a real MSVC-compiled game. See
    LARGE_STRUCT_RETURN_TYPES above.
    """
    return _bare_return_type_name(return_type) in LARGE_STRUCT_RETURN_TYPES


def abi_real_name(cname: str, func: Function) -> str:
    """
    The globally-unique extern "C" symbol name for the real GCC-ABI implementation
    backing the register-swap trampoline for the given interface method.
    """
    return f"oovr_abi_real_{cname}_{func.name}"


def abi_real_params(cname: str, func: Function) -> str:
    """
    The parameter list for a function's ABI trampoline real-implementation, i.e. the
    original member function's args with an explicit leading `self` parameter -
    without leaving a dangling comma when the original function takes no arguments.
    """
    args = func.args_str()
    return f"{cname} *self, {args}" if args else f"{cname} *self"


def write_header(filename, iface):
    header = open(filename, "w", newline='\n')
    header.write("#pragma once\n")
    header.write('#include "BaseCommon.h"\n')

    for i in iface.versions:
        header.write(f'#include "{i.header_filename()}"\n')

    for i in iface.versions:
        _write_interface_header(header, i)

    header.close()


def _write_interface_header(fi, iface: InterfaceDef):
    cname = iface.proxy_class_name()

    # Functions needing the ABI trampoline (see LARGE_STRUCT_RETURN_TYPES) have their
    # real GCC-ABI implementation generated as an extern "C" free function rather than
    # a member of the class (so the naked trampoline can jump to it by a plain,
    # unmangled symbol name). It still needs access to the private `base` member, so
    # forward-declare it ahead of the class and friend it from inside.
    abi_funcs = [func for func in iface.functions if needs_abi_trampoline(func.return_type)]

    fi.write(f"class {cname};\n")
    for func in abi_funcs:
        fi.write(f"extern \"C\" {func.return_type} {abi_real_name(cname, func)}({abi_real_params(cname, func)});\n")

    fi.write(f"""
#include "Reimpl/{iface.base_header()}"
class {cname} : public {iface.namespace()}::{iface.interface()}, public CVRCommon {{
private:
    const std::shared_ptr<{iface.basename()}> base;
public:
    virtual void** _GetStatFuncList() override;
    virtual void Delete() override;
    {cname}();
    // Interface methods:
""".replace("    ", "\t").strip())
    # Note we're doing this strip-then-write-newline dance to get the files to be exactly the same as the old
    # ones as the easy way to make sure nothing breaks during the transition.
    # Later on this can be pulled out, a little whitespace doesn't matter.
    fi.write("\n")

    for func in iface.functions:
        fi.write(f"\t{func.return_type} {func.name}({func.args_str()}) override;\n")

    if abi_funcs:
        fi.write("\t// ABI trampoline real implementations (see LARGE_STRUCT_RETURN_TYPES):\n")
        for func in abi_funcs:
            fi.write(f"\tfriend {func.return_type} {abi_real_name(cname, func)}({abi_real_params(cname, func)});\n")

    fi.write("};\n")


def write_stubs(fi, iface: InterfaceSpec):
    first_ver = iface.versions[0]
    cls = first_ver.basename()
    var = "_single_inst_" + first_ver.varname()
    getter_name = first_ver.getter_name()

    # Write the version-independent code that creates the instance of the base class
    #
    # NOTE on {var}_mutex: {var} (a std::weak_ptr) and {var}_unsafe (its raw
    # pointer shadow) are process-wide singletons written from
    # GetCreate{getter_name}() and from the shared_ptr's own deleter lambda,
    # and read from Get{getter_name}(). Nothing here was ever atomic or
    # lock-protected, so two threads racing a cache-miss for the same OpenVR
    # interface (this can and does happen - see the interfaces_mutex comment
    # in openvr_api.cpp: VR_GetGenericInterface only locks around the
    # `interfaces` map itself, not around CreateInterfaceByName(), so two
    # threads that both miss the cache both proceed to `new {cname}()`
    # concurrently and thus both call GetCreate{getter_name}() concurrently)
    # can interleave a std::weak_ptr::operator= on one thread with a
    # std::weak_ptr::lock() or another operator= on another thread. That's a
    # data race on ordinary (non-atomic) memory - UB - and in practice tears
    # the weak_ptr's internal pointer/control-block fields, so a *later*,
    # perfectly single-threaded caller can construct a shared_ptr from the
    # torn weak_ptr and get a garbage/non-null-but-invalid .get() pointer.
    # This matches corruption observed in the wild as a CVRSystem_XXX::base
    # (a `const std::shared_ptr<BaseSystem>` set exactly once, at
    # construction, from GetCreateBaseSystem()) pointing at garbage - with no
    # single deterministic repro, because it depends on how the two threads'
    # weak_ptr writes interleave. Guard all of {var}/{var}_unsafe with one
    # mutex so construction (and the paired lock()/lookup) of this singleton
    # is atomic end-to-end, not just the final map insert in openvr_api.cpp.
    fi.write(f"""
#include "GVR{iface.name}.gen.h"
// Single inst of {cls}
static std::weak_ptr<{cls}> {var};
static {cls} *{var}_unsafe = NULL;
static std::mutex {var}_mutex;
std::shared_ptr<{cls}> Get{getter_name}() {{
    std::lock_guard<std::mutex> lock({var}_mutex);
    return {var}.lock();
}};
{cls}* GetUnsafe{getter_name}() {{ return {var}_unsafe; }};
std::shared_ptr<{cls}> GetCreate{getter_name}() {{
    std::lock_guard<std::mutex> lock({var}_mutex);
    std::shared_ptr<{cls}> ret = {var}.lock();
    if(!ret) {{
        ret = std::shared_ptr<{cls}>(new {cls}(), []({cls} *obj){{
            std::lock_guard<std::mutex> lock({var}_mutex);
            {var}_unsafe = NULL;
            delete obj;
        }});
        {var} = ret;
        {var}_unsafe = ret.get();
    }}
    return ret;
}}
""".replace("    ", "\t").lstrip())

    # Write the actual version-specific stubs:
    for ver in iface.versions:
        cname = ver.proxy_class_name()
        namespace = ver.namespace()

        fi.write(f"""
// Misc for {cname}:
{cname}::{cname}() : base(GetCreate{ver.getter_name()}()) {{}}
// Interface methods for {cname}:
""".lstrip())

        # Write out all the functions
        for f in ver.functions:
            # If the user implemented this function, don't generate our version of it too
            if f.user_implemented:
                continue

            # Generate an argument string with casts as appropriate
            nargs = []
            for a in f.args:
                if namespace in a.type:
                    casttype = a.type.replace(namespace + "::", "OOVR_")
                    nargs.append("(%s) %s" % (casttype, a.name))
                else:
                    nargs.append(a.name)

            nargs = ", ".join(nargs)

            # Generate the code for the return, with a cast if necessary
            return_str = "return"
            if namespace in f.return_type:
                return_str += f" ({f.return_type})"

            log_stmt = ("\tif (oovr_global_configuration.LogAllOpenVRCalls())\n"
                        f"\t\tOOVR_LOG(\"Entered function (from interface {ver.namespace()})\");\n")

            if needs_abi_trampoline(f.return_type):
                # See LARGE_STRUCT_RETURN_TYPES: this method returns a struct/union
                # too large for RAX, which real MSVC-compiled games calling through
                # the raw vtable pass differently to how GCC/mingw expects it. The
                # real implementation goes in a separate extern "C" free function,
                # and the override that actually occupies the vtable slot is a naked
                # trampoline that swaps RCX/RDX (this <-> hidden return pointer) and
                # tail-jumps into it. Do NOT call `{cname}::{f.name}` from any other
                # GCC-compiled code (e.g. don't add a normal method-call site) - that
                # would hit the trampoline with the wrong (GCC-native) register
                # layout and corrupt things exactly the same way the bug does for
                # real games. Internal code (e.g. the FnTable stub below) must call
                # {abi_real_name(cname, f)}(...) directly instead.
                real_name = abi_real_name(cname, f)
                bare = _bare_return_type_name(f.return_type)
                size = LARGE_STRUCT_RETURN_TYPES[bare]
                fi.write(
                    f"static_assert(sizeof({f.return_type}) == {size}, "
                    f"\"{bare} layout changed - re-verify/update the vtable ABI trampoline "
                    f"in scripts/stubs/codegen.py (LARGE_STRUCT_RETURN_TYPES)\");\n")
                fi.write(f"extern \"C\" {f.return_type} {real_name}({abi_real_params(cname, f)}) {{\n"
                         f"{log_stmt}"
                         f"\t{return_str} self->base->{f.name}({nargs});\n}}\n")
                fi.write(f"__attribute__((naked)) {f.return_type} {cname}::{f.name}({f.args_str()}) {{\n"
                         "\t__asm__ volatile(\n"
                         "\t\t\"mov %rcx, %rax\\n\\t\"\n"
                         "\t\t\"mov %rdx, %rcx\\n\\t\"\n"
                         "\t\t\"mov %rax, %rdx\\n\\t\"\n"
                         f"\t\t\"jmp {real_name}\\n\\t\"\n"
                         "\t);\n}\n")
            else:
                fi.write(f"{f.return_type} {cname}::{f.name}({f.args_str()}) {{\n"
                         f"{log_stmt}"
                         f"\t{return_str} base->{f.name}({nargs});\n}}\n")

        # Generate the fntable
        _build_fntable(fi, ver)

        # Generate the deleter (see BaseCommon.h for the rationale here)
        fi.write(f"void {cname}::Delete() {{ delete this; }}\n")


def _build_fntable(fi, ver: InterfaceDef):
    cname = ver.proxy_class_name()
    prefix = f"fntable_{ver.varname()}_{ver.version}"
    inst_name = f"{prefix}_instance"
    func_array_name = f"{prefix}_funcs"
    func_name_template = f"{prefix}_impl_%s"

    fi.write(f"// FnTable for {cname}:\n")
    fi.write(f"static {cname} *{inst_name} = NULL;\n")

    # Generate the stub functions
    for func in ver.functions:
        if needs_abi_trampoline(func.return_type):
            # This method's vtable slot (inst_name->func.name) is a naked MSVC-ABI
            # register-swap trampoline (see LARGE_STRUCT_RETURN_TYPES and write_stubs)
            # meant only for real games calling through the raw C++ vtable. FnTable
            # access is a flat C function-pointer table, not the C++ vtable, so this
            # internal, GCC-compiled call site must bypass the trampoline entirely and
            # call the real GCC-ABI implementation directly.
            arg_names = func.args_names()
            real_call_args = f"{inst_name}, {arg_names}" if arg_names else inst_name
            call_stmt = f"return {abi_real_name(cname, func)}({real_call_args});"
        else:
            call_stmt = f"return {inst_name}->{func.name}({func.args_names()});"

        fi.write(
            f"static {func.return_type} OPENVR_FNTABLE_CALLTYPE {func_name_template % func.name}({func.args_str()}) {{ {call_stmt} }}\n")

    # Generate the array
    fi.write("static void *%s[] = {\n" % func_array_name)
    for func in ver.functions:
        fi.write("\t(void*) %s,\n" % func_name_template % func.name)

    fi.write("};\n")

    # Generate the getter
    fi.write(
        f"void** {cname}::_GetStatFuncList() {{ {inst_name} = this; return {func_array_name}; }}\n")


def write_stub_footer(fi, interfaces: List[InterfaceSpec]):
    # Generate CreateInterfaceByName
    fi.write("// Get interface by name\n")
    fi.write("void *CreateInterfaceByName(const char *name) {\n")

    for spec in interfaces:
        for ver in spec.versions:
            fi.write(
                f"\tif(strcmp({ver.version_variable()}, name) == 0) return new {ver.proxy_class_name()}();\n")

    fi.write("\treturn NULL;\n")
    fi.write("}\n")

    # Generate the flag stuff
    fi.write("// Get flags by name\n")
    fi.write(
        "uint64_t GetInterfaceFlagsByName(const char *name, const char *flag, bool *success) {\n")
    fi.write("\tif(success) *success = true;\n")

    for spec in interfaces:
        for ver in spec.versions:
            cflags = dict()

            base_flags = spec.flags
            for key in base_flags:
                if key[0] == "[" and key[-1] == "]":
                    cflags[key[1:-1]] = base_flags[key]

            for flag in ver.flags:
                match = cflag_spec.match(flag)
                if not match:
                    continue

                name = match.group("name")
                value = match.group("value")
                if value:
                    cflags[name] = value
                else:
                    del cflags[name]

            if not cflags:
                continue

            fi.write(f"\tif(strcmp({ver.version_variable()}, name) == 0) {{\n")
            for name in cflags:
                val = cflags[name]
                fi.write("\t\tif(strcmp(\"%s\", flag) == 0) return (%s);\n" % (name, val))
            fi.write("\t}\n")

    fi.write("\tif(success) *success = false;\n")
    fi.write("\treturn 0;\n")
    fi.write("}\n")
