// gcc -shared -fPIC test.c -o libtest.so

#include <stdlib.h>
#include <string.h>

struct student {
    int age;
    char name[0];
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
