/** Decode a hexadecimal digit.

    Below, the malformed bits exercise the warning paths in the
    HTML-table parser: stray text directly inside the table and
    inside a row, plus an unexpected paragraph element where a
    cell is expected.

    <table>
      stray text inside table
      <p>not a row</p>
      <thead>
        stray text inside thead
        <tr>
          <th>Digit</th>
          <th>Value</th>
        </tr>
      </thead>
      <tr>
        stray text inside row
        <td>A</td>
        <td>10</td>
        <p>Not a cell</p>
      </tr>
      <tr>
        <td>B</td>
        <td>11</td>
      </tr>
    </table>
 */
int hexDigit(char c);
