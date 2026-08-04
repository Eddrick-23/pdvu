# PDF parser test fixtures

| File               | Expected behavior                                                                                  |
|--------------------|----------------------------------------------------------------------------------------------------|
| `single_page.pdf`  | Valid PDF; 1 page; MediaBox 200 x 300 points; visible black rectangle, text, and blue line.        |
| `multi_page.pdf`   | Valid PDF; 3 pages sized 200 x 300, 400 x 100, and 150 x 150 points.                               |
| `rotated_page.pdf` | Valid PDF; 1 source page sized 300 x 200 points with `/Rotate 90`; displayed bounds are 200 x 300. |
| `not_a_pdf.pdf`    | Deliberately invalid PDF; loading should fail cleanly without crashing.                            |

Suggested uses:

- Use `single_page.pdf` for basic loading, display-list lifetime, and RGB rendering tests.
- Use `multi_page.pdf` for page count, indexing, per-page dimensions, and parser duplication.
- Use `rotated_page.pdf` for intrinsic PDF rotation behavior.
- Use `not_a_pdf.pdf` for graceful error handling.
