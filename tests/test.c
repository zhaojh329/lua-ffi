// gcc -shared -fPIC test.c -o libtest.so

#include <stdlib.h>
#include <string.h>

struct student {
    int age;
    char name[0];
};

struct bitfield_u {
    unsigned int a:3;
    unsigned int b:5;
    unsigned int c:6;
};

struct bitfield_s {
    int a:3;
    int b:5;
};

struct nested_bitfield_outer {
    struct bitfield_u inner;
    int x;
};

int student_get_age(struct student st)
{
    return st.age;
}

int student_get_age_ptr(struct student *st)
{
    return st->age;
}

const char *student_get_name(struct student *st)
{
    return st->name;
}

struct student *student_new(int age, const char *name)
{
    struct student *st = malloc(sizeof(struct student) + strlen(name) + 1);
    st->age = age;
    strcpy(st->name, name);
    return st;
}

int *pass_array(int a[])
{
    return a;
}

int bitfield_u_matches(struct bitfield_u *bf, unsigned int a, unsigned int b, unsigned int c)
{
    return bf->a == a && bf->b == b && bf->c == c;
}

void bitfield_u_set(struct bitfield_u *bf, unsigned int a, unsigned int b, unsigned int c)
{
    bf->a = a;
    bf->b = b;
    bf->c = c;
}

int bitfield_s_matches(struct bitfield_s *bf, int a, int b)
{
    return bf->a == a && bf->b == b;
}

void bitfield_s_set(struct bitfield_s *bf, int a, int b)
{
    bf->a = a;
    bf->b = b;
}

int nested_bitfield_total(struct nested_bitfield_outer outer)
{
    return outer.inner.a + outer.inner.b + outer.inner.c + outer.x;
}

struct nested_bitfield_outer nested_bitfield_make(void)
{
    struct nested_bitfield_outer outer = {{1, 2, 3}, 4};

    return outer;
}

int cb_mul10(int i)
{
    return i * 10;
}

int call_f0(int (*cb)(int))
{
    return cb(20);
}

int call_f1(int (*cb)(int), int x)
{
    return cb(x);
}

int call_f2(int (*cb)(int i), int x)
{
    return cb(x);
}

int call_f3(int x, int (*cb)(int i))
{
    return cb(x);
}

int call_f4(int x, int (*cb)(int i))
{
    return cb(x);
}
