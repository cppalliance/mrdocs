/** The main information about a person

    This class represents a person with
    a name and an age. This is the
    information about a person that
    we need to store in our system.
*/
struct person
{
    /// The person's full name.
    char const* name;

    /// The person's age in years.
    int age;
};

/** A function to greet a person

    This function takes a person and
    prints a greeting message.

    @param p The person to greet
*/
void greet(person const& p);
