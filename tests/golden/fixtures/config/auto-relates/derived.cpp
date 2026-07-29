/** A base class for non-member functions
 */
struct ABase {};

/** A concrete implementation for ABase
 */
struct A : ABase {};

/** A view of A
 */
struct AView : ABase {};

/** Another view of A

    Used to test indirect derived classes
*/
struct AView2 : AView {};

/** A non-member function of ABase
 */
void f1(ABase const&);

/** A non-member function of ABase
 */
void f2(ABase&);

/** A non-member function of ABase
 */
void f3(ABase const*);

/** A non-member function of ABase
 */
void f4(ABase*);

/** A non-member function of ABase
 */
void f5(ABase* const);

/** A non-member function of ABase only
 */
void n(ABase);


