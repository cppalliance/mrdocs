/** Uses the small-buffer optimization[^sbo] to avoid heap traffic.

    [^sbo]: Short buffers live inline in the object, so no allocation happens
    until the capacity is exceeded.
 */
void configure();
