/** Compute the exclusive-or of two boolean values.

    Truth table:

    <table>
      <tr>
        <th>a</th>
        <th>b</th>
        <th>a XOR b</th>
      </tr>
      <tr>
        <td>0</td>
        <td>0</td>
        <td>0</td>
      </tr>
      <tr>
        <td>0</td>
        <td>1</td>
        <td>1</td>
      </tr>
      <tr>
        <td>1</td>
        <td>0</td>
        <td>1</td>
      </tr>
      <tr>
        <td>1</td>
        <td>1</td>
        <td>0</td>
      </tr>
    </table>
 */
bool xor_(bool a, bool b);
