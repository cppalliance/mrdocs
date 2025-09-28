namespace boost::openmethod {
    template<class Arg>
    auto final_virtual_ptr(Arg&& obj);

    template<class Registry, typename Arg>
    auto final_virtual_ptr(Arg&& obj);

    namespace policies::runtime_checks
    {
        /** Documentation with ref to final_virtual_ptr

            See @ref final_virtual_ptr
         */
        struct runtime_checks;
    }
}
