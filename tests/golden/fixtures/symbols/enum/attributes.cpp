// Attributes are captured on enums and their enumerators.
enum class [[deprecated("use Color2")]] Color {
    Red,
    Green [[deprecated("use Blue")]],
    Blue
};
