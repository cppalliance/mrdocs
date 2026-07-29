/** Resamples an audio signal to a new sample rate.

    @tparam Sample The sample type, usually `float` or `short`.
    @param[in]  in    The input samples.
    @param[in]  count Number of input samples.
    @param[in]  ratio Output rate divided by input rate.
    @param[out] out   Buffer that receives the resampled signal.
    @returns The number of samples written to `out`.
 */
template <class Sample>
unsigned resample(Sample const* in, unsigned count, double ratio, Sample* out);
