namespace A {
    // This namespace and everything in it should be excluded
    namespace B {
        class C {
            public:
                void f();
                virtual void g();
                virtual void h() = 0;
        };
    }

    // B::C and its members will be extracted as
    // dependencies so that this information can
    // be used by D.
    class D : public B::C {
        public:
            void g() override;
            void h() override;
            void i();
    };

    // B::C and its members will be extracted as
    // dependencies so that this information can
    // be used by D.
    // g() is not defined and is only inherited
    // from B::C
    class E : public B::C {
        public:
            void h() override;
            void i();
    };
}
