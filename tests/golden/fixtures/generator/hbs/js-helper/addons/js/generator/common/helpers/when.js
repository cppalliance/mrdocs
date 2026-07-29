// Block helper that exercises options.fn/options.inverse.
function when(condition, options)
{
    if (Array.isArray(condition))
    {
        condition = condition.length ? condition[0] : undefined;
    }
    if (arguments.length < 2 || !options || typeof options !== 'object')
        return '';

    if (condition)
        return options.fn ? options.fn(this) : '';
    return options.inverse ? options.inverse(this) : '';
}
