// Shared utility functions for JavaScript helpers.
// Files starting with '_' are loaded before helper files and define
// globals that can be used by all helpers.

/**
 * Normalize Handlebars arguments by flattening arrays and filtering
 * out objects (which are typically the options hash).
 * @param {Arguments|Array} args - The arguments to normalize.
 * @returns {Array} - Filtered array of primitive values.
 */
function normalize_args(args)
{
    var list = [];
    for (var i = 0; i < args.length; ++i)
        list.push(args[i]);

    if (list.length === 1 && Array.isArray(list[0]))
    {
        list = list[0];
    }

    var filtered = [];
    for (var j = 0; j < list.length; ++j)
    {
        var v = list[j];
        if (v === "[object Object]")
            continue;
        if (v && typeof v === 'object' && !Array.isArray(v))
            continue;
        filtered.push(v);
    }
    return filtered;
}

/**
 * Format an object's key-value pairs as a sorted, comma-separated string.
 * @param {Object} obj - The object to format.
 * @returns {string} - Formatted string like "key1=val1,key2=val2".
 */
function format_object(obj)
{
    var keys = [];
    for (var k in obj)
    {
        if (Object.prototype.hasOwnProperty.call(obj, k))
            keys.push(k);
    }
    keys.sort();
    var parts = [];
    for (var i = 0; i < keys.length; ++i)
    {
        var key = keys[i];
        parts.push(key + '=' + obj[key]);
    }
    return parts.join(',');
}
