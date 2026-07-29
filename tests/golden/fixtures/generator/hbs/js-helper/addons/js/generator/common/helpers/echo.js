// Echo helper used in golden tests; keeps output stable across engines.
// Uses normalize_args from _utils.js (loaded before helper files).

function echo()
{
    var args = normalize_args(arguments);
    var value = args.length > 0 ? args[0] : '';
    return 'js:' + value;
}
