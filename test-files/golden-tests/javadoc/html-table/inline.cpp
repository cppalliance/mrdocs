/** Format a temperature value with the requested precision.

    Each cell holds inline content other than plain text, so the
    cell-parser must dispatch through the regular inline visitor:

    <table>
      <tr>
        <th>Format</th>
        <th>Sample</th>
        <th>Reference</th>
      </tr>
      <tr>
        <td><em>fixed</em></td>
        <td>23.50</td>
        <td><a href="https://en.cppreference.com/w/cpp/io/manip/fixed">cppreference</a></td>
      </tr>
      <tr>
        <td><em>scientific</em></td>
        <td>2.35e+01</td>
        <td><a href="https://en.cppreference.com/w/cpp/io/manip/scientific">cppreference</a></td>
      </tr>
    </table>
 */
double formatTemperature(double value, int precision);
