// A URL-safe slug: lowercased, runs of non-alphanumeric characters collapsed
// to a single `-`, and leading and trailing dashes removed.
function slug(name)
{
    if (!name) {
        return '';
    }
    return String(name)
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, '-')
        .replace(/^-+|-+$/g, '');
}
