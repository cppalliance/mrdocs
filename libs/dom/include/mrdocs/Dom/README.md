# include/mrdocs/Dom/

The document object model: the dynamic value types (`Value`, `Object`, `Array`,
`String`, `Function`) that templates and generators traverse. `LazyObject` and
`LazyArray` defer materialization so large symbol sets are only realized when a
template touches them.
