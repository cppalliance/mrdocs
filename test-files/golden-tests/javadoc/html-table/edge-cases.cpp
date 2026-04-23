/** Function whose doc paragraph contains both prose and an
    inline table on the same paragraph (no blank line in
    between), so the table interrupts an in-flight paragraph
    and the accumulated inlines are flushed before the table.

    Inline table follows: <table><tr><td>1</td><td>2</td></tr></table>
 */
void f1();

/** Function whose doc contains a table nested inside a cell
    of an outer table. Exercises the depth tracking of the
    end-tag matcher.

    <table>
      <tr>
        <td>
          <table>
            <tr>
              <td>nested</td>
            </tr>
          </table>
        </td>
        <td>outer</td>
      </tr>
    </table>
 */
void f2();

/** Function whose doc contains empty cells.

    <table>
      <tr>
        <th>Name</th>
        <th></th>
      </tr>
      <tr>
        <td>foo</td>
        <td></td>
      </tr>
    </table>
 */
void f3();
