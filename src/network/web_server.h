#pragma once

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

void initializeWebServer();

extern const char index_html[] asm("_binary_data_index_html_start");
extern const char js_app_js[] asm("_binary_data_js_app_js_start");
extern const char styles_css[] asm("_binary_data_styles_css_start");

struct webpage {
    const char filename[30];
    const char *data;
    const char content_type[30];
};

static const webpage webpages[] = {
    {"/", index_html, "text/html"},
    {"/index", index_html, "text/html"},
    {"/index.html", index_html, "text/html"},
    {"/js/app.js", js_app_js, "application/javascript"},
    {"/styles.css", styles_css, "text/css"},
};

#endif