template<typename T>
struct A 
{ 
    void f(); 
};

template<>
struct A<int> 
{ 
    void g(); 
};

template<>
struct A<long> 
{ 
    void h(); 
};

template<typename T>
struct B
{ 
    void f(); 
};

template<typename T>
struct B<T*> 
{ 
    void g(); 
};

template<typename T>
struct B<T&>
{ 
    void h(); 
};

template<typename T, typename U>
struct C
{ 
    void f(); 
};

template<>
struct C<int, int>
{ 
    void g(); 
};

template<typename T>
struct C<T*, int>
{ 
    void h(); 
};
