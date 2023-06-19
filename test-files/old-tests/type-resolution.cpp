struct A { };

template<typename T, typename U>
struct B { };

using C = A;

using D = B<short, long>;

template<typename T>
using E = B<T, long>;

void f0(A);
void f1(const A);
void f2(A&);
void f3(const A&);
void f4(A*);
void f5(const A*);
void f6(A**);
void f7(const A**);
void f8(const A* const*);

void g0(C);
void g1(const C);
void g2(C&);
void g3(const C&);
void g4(C*);
void g5(const C*);
void g6(C**);
void g7(const C**);
void g8(const C* const*);

void h0(B<short, long>);
void h1(const B<short, long>);
void h2(B<short, long>&);
void h3(const B<short, long>&);
void h4(B<short, long>*);
void h5(const B<short, long>*);
void h6(B<short, long>**);
void h7(const B<short, long>**);
void h8(const B<short, long>* const*);

void i0(D);
void i1(const D);
void i2(D&);
void i3(const D&);
void i4(D*);
void i5(const D*);
void i6(D**);
void i7(const D**);
void i8(const D* const*);

void j0(E<short>);
void j1(const E<short>);
void j2(E<short>&);
void j3(const E<short>&);
void j4(E<short>*);
void j5(const E<short>*);
void j6(E<short>**);
void j7(const E<short>**);
void j8(const E<short>* const*);
