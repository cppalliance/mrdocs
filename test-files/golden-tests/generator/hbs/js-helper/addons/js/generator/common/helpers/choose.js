// Block helper exercising options.fn/options.inverse.
function choose(options)
{
    if (!options || typeof options !== 'object')
        return 'otherwise';
    return options.inverse ? options.inverse(this) : 'otherwise';
}
