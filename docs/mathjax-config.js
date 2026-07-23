window.MathJax =
{
  tex:
  {

    inlineMath: [['$', '$'], ['\\(', '\\)']],
    displayMath: [['$$', '$$'], ['\\[', '\\]']],
    processEscapes: true,      // allow an escaped dollar sign as literal text.
    processEnvironments: true  // accept LaTeX environments inside math.
  },
  options:
  {
    ignoreHtmlClass: 'tex2jax_ignore',
    processHtmlClass: 'tex2jax_process'
  },
  output:
  {
    font: 'mathjax-fira'
  },
  chtml:
  {
    displayAlign: 'left',
    displayIndent: '2em'
  },
  svg:
  {
    displayAlign: 'left',
    displayIndent: '2em'
  }
};
