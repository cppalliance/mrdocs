# Handlebars generator fixtures

These tests exercise the Handlebars generators (adoc, html). Each feature gets a small directory that renders the basics plus any case that was controversial or regressed once. They stay minimal, since the XML tests already give exhaustive coverage.

Each test requests the formats it needs in its `mrdocs.yml`, for example `generator: [adoc, html, xml]`. A feature test usually includes xml so the schema element is covered next to the rendering.
