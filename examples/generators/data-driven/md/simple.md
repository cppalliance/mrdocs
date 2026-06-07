# Reference


## [Point](#Point)

A small two-dimensional point.


### Synopsis

Declared in `<simple.cpp>`
```cpp
struct Point;
```
## Member Functions
<table style="table-layout: fixed; width: 100%;">
<thead>
<tr>
<th>Name</th><th>Description</th>
</tr>
</thead>
<tbody>
<tr>
<td>[`length`](#Point-length) </td><td>Distance from the origin.</td></tr><tr>
<td>[`translate`](#Point-translate) </td><td>Translate by an offset.</td></tr>
</tbody>
</table>




## [Point::length](#Point-length)

Distance from the origin.


### Synopsis

Declared in `<simple.cpp>`
```cpp
double
length() const;
```

## [Point::translate](#Point-translate)

Translate by an offset.


### Synopsis

Declared in `<simple.cpp>`
```cpp
void
translate(
    int dx,
    int dy);
```

