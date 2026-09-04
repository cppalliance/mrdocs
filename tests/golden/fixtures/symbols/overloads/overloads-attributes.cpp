// Attributes are merged onto an overload set from its members: the set carries
// an attribute kind only when every overload has it, and keeps a property value
// only when every overload agrees on it. The set must not borrow a lone
// overload's attribute.

/// Every overload is nodiscard, so the set is nodiscard.
[[nodiscard]] int all_nodiscard();
/// The second overload of an all-nodiscard set.
[[nodiscard]] int all_nodiscard(int);

/// Only this overload is nodiscard, so the set is not.
[[nodiscard]] int some_nodiscard();
/// This overload is not nodiscard.
int some_nodiscard(int);

/// Every overload is deprecated with the same message: the set keeps it.
[[deprecated("gone")]] void same_message();
/// The second overload, deprecated with the same message.
[[deprecated("gone")]] void same_message(int);

/// Every overload is deprecated but with a different message: the set is
/// deprecated with no message.
[[deprecated("use a")]] void diff_message();
/// The second overload, deprecated with a different message.
[[deprecated("use b")]] void diff_message(int);
