/** Look up an HTTP status code's category.

    The categories are grouped using `<thead>`, `<tbody>`, and
    `<tfoot>`; MrDocs treats those as transparent grouping
    elements and renders the rows directly in the table.

    <table>
      <thead>
        <tr>
          <th>Range</th>
          <th>Category</th>
        </tr>
      </thead>
      <tbody>
        <tr>
          <td>1xx</td>
          <td>Informational</td>
        </tr>
        <tr>
          <td>2xx</td>
          <td>Success</td>
        </tr>
        <tr>
          <td>3xx</td>
          <td>Redirection</td>
        </tr>
        <tr>
          <td>4xx</td>
          <td>Client error</td>
        </tr>
        <tr>
          <td>5xx</td>
          <td>Server error</td>
        </tr>
      </tbody>
      <tfoot>
        <tr>
          <td>Other</td>
          <td>Reserved</td>
        </tr>
      </tfoot>
    </table>
 */
char const* httpCategory(int code);
