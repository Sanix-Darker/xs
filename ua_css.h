#ifndef XS_UA_CSS_H
#define XS_UA_CSS_H

/* Built-in user-agent stylesheet (FEAT-015). Lowest cascade priority: applied
   before author CSS so authors can override. Values chosen to match the
   browser's existing visual defaults. Spacing/heading sizes that are currently
   handled directly in layout.c are intentionally limited here to color and
   alignment defaults to avoid double-application; the box model migration
   (FEAT-006) will move spacing here later. */
static const char XS_UA_CSS[] =
    "a { color: #1446b4; }\n"
    "body { color: #1e1e1e; }\n"
    "mark { background: #fff2a8; }\n"
    "h1,h2,h3,h4,h5,h6 { color: #0f0f0f; }\n"
    "th { text-align: center; }\n"
;

#endif /* XS_UA_CSS_H */
