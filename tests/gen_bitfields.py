#!/usr/bin/env python3
"""Generate native C oracles, not expected layouts, for the bitfield tests."""
import pathlib
import sys

cases = []


def add_case(name, fields, packed=False, declaration=None):
    members = []

    for base, field, width in fields:
        suffix = f':{width}' if width else ''
        members.append(f'{base} {field}{suffix}')

    body = '; '.join(members) + ';'
    attribute = ' __attribute__((packed))' if packed else ''
    declaration = declaration or f'struct{attribute} {name} {{ {body} }};'
    cases.append((name, fields, declaration))


add_case('tail', [('unsigned int', 'a', 3), ('unsigned char', 'b', 0)])
add_case('head', [('unsigned char', 'a', 0), ('unsigned int', 'b', 3)])
add_case('tiny', [('unsigned int', 'a', 3), ('unsigned int', 'b', 5)], packed=True)
add_case('comma', [('unsigned int', 'a', 3), ('unsigned int', 'b', 0)],
         declaration='struct comma { unsigned int a:3, b; };')
add_case('boolean', [('bool', 'a', 1), ('bool', 'b', 1)])

for base, bits in [
    ('unsigned char', 8), ('signed char', 8), ('char', 8),
    ('unsigned short', 16), ('short', 16),
    ('unsigned int', 32), ('int', 32),
    ('unsigned long long', 64), ('long long', 64),
]:
    for packed in (False, True):
        for prefix in (False, True):
            for first_width, second_width in ((3, 5), (bits - 1, 2), (1, bits)):
                fields = [('unsigned char', 'lead', 0)] if prefix else []
                fields += [
                    (base, 'a', first_width),
                    (base, 'b', second_width),
                    ('unsigned char', 'tail', 0),
                ]
                add_case(f'case{len(cases)}', fields, packed)

        add_case(f'case{len(cases)}', [
            (base, 'a', 3),
            ('unsigned char', 'mid', 0),
            (base, 'b', 5),
            ('unsigned short', 'tail', 0),
        ], packed)

# long follows the target's data model; do not assume LP64 in the oracle.
for base in ('long', 'unsigned long'):
    for packed in (False, True):
        add_case(f'case{len(cases)}', [
            ('unsigned char', 'lead', 0),
            (base, 'a', 3),
            (base, 'b', 29),
            ('unsigned char', 'tail', 0),
        ], packed)

out = ['''#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct oracle {
    const char *declaration;
    const char *name;
    size_t size;
    size_t alignment;
    int count;
    const char *(*field_name)(int);
    const char *(*field_type)(int);
    long long (*set)(void *, int, long long);
    long long (*get)(const void *, int);
    double (*number)(const void *, int);
    size_t (*offset)(int);
};
''']

for name, fields, declaration in cases:
    out.append(declaration)
    out.append(f'struct align_{name} {{ char c; struct {name} value; }};\n')

    for kind, values in [
        ('name', [field for base, field, width in fields]),
        ('type', [base for base, field, width in fields]),
    ]:
        quoted_values = ', '.join(f'"{value}"' for value in values)
        out.append(f'''static const char *{name}_{kind}(int f)
{{
    static const char *values[] = {{{quoted_values}}};

    return values[f];
}}
''')

    out.append(f'''static long long {name}_set(void *p, int f, long long v)
{{
    struct {name} *s = p;

    switch (f) {{''')
    for index, (base, field, width) in enumerate(fields):
        out.append(f'''    case {index}:
        s->{field} = v;
        return s->{field};''')
    out.append('''    }

    abort();
}
''')

    for kind, return_type in [('get', 'long long'), ('number', 'double')]:
        out.append(f'''static {return_type} {name}_{kind}(const void *p, int f)
{{''')
        if kind == 'get':
            out.append(f'    const struct {name} *s = p;')

        out.append('\n    switch (f) {')
        for index, (base, field, width) in enumerate(fields):
            # Clang 14 on MIPS loses packed alignment on direct float conversion.
            # Restore the base type so unsigned 64-bit values keep their range.
            value = f's->{field}' if kind == 'get' else f'({base}){name}_get(p, f)'
            out.append(f'''    case {index}:
        return {value};''')
        out.append('''    }

    abort();
}
''')

    out.append(f'''static size_t {name}_offset(int f)
{{
    struct {name} s;
    size_t i;

    memset(&s, 0, sizeof(s));
    switch (f) {{''')
    for index, (base, field, width) in enumerate(fields):
        if width:
            # Clang 14 on MIPS misaligns direct accesses to packed stack bitfields.
            # Reuse the pointer-based setter to keep native writes alignment-safe.
            out.append(f'''    case {index}:
        {name}_set(&s, {index}, -1);
        break;''')
        else:
            out.append(f'''    case {index}:
        return offsetof(struct {name}, {field});''')
    out.append('''    }

    for (i = 0; i < sizeof(s); i++)
        if (((unsigned char *)&s)[i])
            return i;

    abort();
}
''')

out.append('static const struct oracle cases[] = {')
for name, fields, declaration in cases:
    full_declaration = declaration + f' struct align_{name} {{ char c; struct {name} value; }};'
    out.append(f'''    {{
        "{full_declaration}", "{name}",
        sizeof(struct {name}), __alignof__(struct {name}), {len(fields)},
        {name}_name, {name}_type,
        {name}_set, {name}_get, {name}_number, {name}_offset
    }},''')
out.append('};')
out.append('''
int bf_count(void) { return sizeof(cases) / sizeof(cases[0]); }

const char *bf_decl(int c) { return cases[c].declaration; }

const char *bf_name(int c) { return cases[c].name; }

size_t bf_size(int c) { return cases[c].size; }

size_t bf_align(int c) { return cases[c].alignment; }

int bf_fields(int c) { return cases[c].count; }

const char *bf_field(int c, int f) { return cases[c].field_name(f); }

const char *bf_type(int c, int f) { return cases[c].field_type(f); }

size_t bf_offset(int c, int f) { return cases[c].offset(f); }

long long bf_set(int c, void *p, int f, long long v) { return cases[c].set(p, f, v); }

long long bf_get(int c, const void *p, int f) { return cases[c].get(p, f); }

double bf_number(int c, const void *p, int f) { return cases[c].number(p, f); }

void bf_pattern(int c, void *p, int f, unsigned char byte)
{
    cases[c].set(p, f, (long long)(UINT64_C(0x0101010101010101) * byte));
}

/* Compare defined values only: compiler stores may leave padding unspecified. */
int bf_equal(int c, const void *a, const void *b)
{
    int f;

    for (f = 0; f < cases[c].count; f++)
        if (cases[c].get(a, f) != cases[c].get(b, f))
            return 0;

    return 1;
}

/* Exact-size heap allocations expose overreads to ASan. */
void *bf_alloc(int c) { return calloc(1, cases[c].size); }

void bf_free(void *p) { free(p); }

/* A packed byte at the end of a readable page must not touch the next page. */
void *bf_guard(void)
{
    long page = sysconf(_SC_PAGESIZE);
    unsigned char *p = mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (p == MAP_FAILED)
        return NULL;

    if (mprotect(p + page, page, PROT_NONE)) {
        munmap(p, page * 2);
        return NULL;
    }

    return p + page - 1;
}

void bf_unguard(void *p)
{
    long page = sysconf(_SC_PAGESIZE);

    munmap((unsigned char *)p - page + 1, page * 2);
}
''')
pathlib.Path(sys.argv[1]).write_text('\n'.join(out))
