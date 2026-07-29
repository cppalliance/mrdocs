// Describe helper: reports type and value in a deterministic string.
// Uses normalize_args and format_object from _utils.js (loaded before helper files).

function describe()
{
    var list = normalize_args(arguments);
    var type;
    var value;

    if (list.length === 0)
    {
        type = 'undefined';
        value = '';
    }
    else if (list.length > 1)
    {
        type = 'array';
        value = list.join(',');
    }
    else
    {
        var v = list[0];
        if (v === null)
        {
            type = 'null';
            value = '';
        }
        else if (Array.isArray(v))
        {
            type = 'array';
            value = v.join(',');
        }
        else
        {
            type = typeof v;
            if (type === 'object')
                value = format_object(v);
            else
                value = String(v);
        }
    }

    return type + ':' + value;
}
