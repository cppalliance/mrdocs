namespace app {

/// A user-defined literal for kilometers.
long double operator""_km(long double v);

/// A user-defined literal for a repeat count.
unsigned long long operator""_times(unsigned long long n);

}
