// Glue helper: flattens positional args (arrays allowed) and joins with the first argument as separator.
// Uses normalize_args from _utils.js (loaded before helper files).

function glue()
{
    var list = normalize_args(arguments);
    if (list.length === 0)
        return '';

    var sep = list[0];
    var items = [];
    for (var i = 1; i < list.length; ++i)
    {
        var v = list[i];
        if (Array.isArray(v))
        {
            for (var j = 0; j < v.length; ++j)
                items.push(v[j]);
        }
        else
        {
            items.push(v);
        }
    }
    return items.join(sep);
}
