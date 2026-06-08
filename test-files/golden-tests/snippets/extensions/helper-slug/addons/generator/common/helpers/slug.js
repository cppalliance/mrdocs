// slug(name): URL-safe slug for a string.
//
// Lowercases, collapses runs of non-alphanumeric characters into a
// single `-`, and strips leading/trailing dashes. Registered in
// `generator/common/` so every template-driven generator can reuse it.
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
