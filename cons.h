struct SEXPR {
    struct SEXPR * sexpr_car;
    struct SEXPR * sexpr_cdr;
};

typedef struct SEXPR * sexpr;

extern sexpr cons();

# define nil ((sexpr) 0)
# define car(x) ((x) -> sexpr_car)
# define cdr(x) ((x) -> sexpr_cdr)
# define null(x) ((x) == nil)

# define head(x) car(x)
# define tail(x) cdr(x)

# define caar(x) car(car(x))
# define cadr(x) car(cdr(x))
# define cddr(x) cdr(cdr(x))
# define cdar(x) cdr(car(x))
# define caddr(x) car(cdr(cdr(x)))

# define first(x) car(x)
# define second(x) cadr(x)
# define third(x) caddr(x)

# define list1(x) cons(x, nil)
# define list2(x,y) cons(x, cons(y, nil))
# define list3(x,y,z) cons(x, cons(y, cons(z, nil)))
