struct Optional
{
    template <typename Self>
    constexpr auto&& value(this Self&& self);

    template <typename Self>
    constexpr auto&& value(this Self&& self, int x);
};
