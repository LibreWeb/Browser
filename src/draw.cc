#include "draw.h"
#include "node.h"
#include "mainwindow.h"
#include <gdk/gdkthreads.h>
#include <gdk/gdkselection.h>
#include <iostream>
#define PANGO_SCALE_XXX_LARGE ((double)1.98)

struct DispatchData
{
    GtkTextBuffer *buffer;
    std::string text;
    std::string url;
};

Draw::Draw(MainWindow &mainWindow)
    : mainWindow(mainWindow),
      buffer(Glib::unwrap(this->get_buffer())),
      fontSize(10 * PANGO_SCALE),
      fontFamily("Ubuntu Monospace"),
      headingLevel(0),
      listLevel(0),
      isBold(false),
      isItalic(false),
      bulletListLevel(0),
      orderedListLevel(0),
      isOrderedList(false),
      isLink(false),
      defaultFont(fontFamily),
      bold(fontFamily),
      italic(fontFamily),
      boldItalic(fontFamily),
      heading1(fontFamily),
      heading2(fontFamily),
      heading3(fontFamily),
      heading4(fontFamily)
{
    set_editable(false);
    set_indent(15);
    set_left_margin(10);
    set_right_margin(10);
    set_top_margin(5);
    set_bottom_margin(5);
    set_monospace(true);
    set_cursor_visible(false);
    set_app_paintable(true);

    defaultFont.set_size(fontSize);
    bold.set_size(fontSize);
    bold.set_weight(Pango::WEIGHT_BOLD);
    italic.set_size(fontSize);
    italic.set_style(Pango::Style::STYLE_ITALIC);
    boldItalic.set_size(fontSize);
    boldItalic.set_weight(Pango::WEIGHT_BOLD);
    boldItalic.set_style(Pango::Style::STYLE_ITALIC);

    heading1.set_size(fontSize * PANGO_SCALE_XXX_LARGE);
    heading1.set_weight(Pango::WEIGHT_BOLD);
    heading2.set_size(fontSize * PANGO_SCALE_XX_LARGE);
    heading2.set_weight(Pango::WEIGHT_BOLD);
    heading3.set_size(fontSize * PANGO_SCALE_X_LARGE);
    heading3.set_weight(Pango::WEIGHT_BOLD);
    heading4.set_size(fontSize * PANGO_SCALE_LARGE);
    heading4.set_weight(Pango::WEIGHT_BOLD);
    heading5.set_size(fontSize * PANGO_SCALE_MEDIUM);
    heading5.set_weight(Pango::WEIGHT_BOLD);
    heading6.set_size(fontSize * PANGO_SCALE_MEDIUM);
    heading6.set_weight(Pango::WEIGHT_BOLD);

    // Click event
    signal_event_after().connect(sigc::mem_fun(this, &Draw::event_after));
}

/**
 * Links can be activated by clicking or touching the screen.
 */
void Draw::event_after(GdkEvent *ev)
{
    gdouble ex, ey;
    Gtk::TextBuffer::iterator iter;
    int x, y;

    if (ev->type == GDK_BUTTON_RELEASE)
    {
        GdkEventButton *event;
        event = (GdkEventButton *)ev;
        if (event->button != GDK_BUTTON_PRIMARY)
            return;
        ex = event->x;
        ey = event->y;
    }
    else if (ev->type == GDK_TOUCH_END)
    {
        GdkEventTouch *event;
        event = (GdkEventTouch *)ev;
        ex = event->x;
        ey = event->y;
    }
    else
        return;

    // Get the textview coordinates and retrieve an iterator
    window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_WIDGET, ex, ey, x, y);
    get_iter_at_location(iter, x, y);
    // Find the links
    followLink(iter);
}

/**
 * Search for links
 */
void Draw::followLink(Gtk::TextBuffer::iterator &iter)
{
    auto tags = iter.get_tags();
    for (auto const &tag : tags)
    {
        char *url = static_cast<char *>(tag->get_data("url"));
        if (url != 0 && (strlen(url) > 0))
        {
            // Get the URL
            mainWindow.doRequest(url, true);
            break;
        }
    }
}

void Draw::showMessage(const std::string &message, const std::string &detailed_info)
{
    this->clear();

    insertHeading1(message);
    insertText(detailed_info);
}

/**
 * Show start page
 */
void Draw::showStartPage()
{
    this->clear();

    insertHeading1("Welcome to the Decentralized Web (DWeb)");
    insertText("See also the: ");
    insertLink("Example page on IPFS", "ipfs://QmQzhn6hEfbYdCfwzYFsSt3eWpubVKA1dNqsgUwci5vHwq");
}

/**
 * Process AST document and draw the text in the GTK TextView
 */
void Draw::processDocument(cmark_node *root_node)
{
    this->clear();

    // Loop over AST nodes
    cmark_event_type ev_type;
    cmark_iter *iter = cmark_iter_new(root_node);
    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE)
    {
        cmark_node *cur = cmark_iter_get_node(iter);
        processNode(cur, ev_type);
    }
}

void Draw::selectAll()
{
    auto buffer = get_buffer();
    buffer->select_range(buffer->begin(), buffer->end());
}

void Draw::cut()
{
    bool isEditable = get_editable();
    if (isEditable)
    {
        auto buffer = get_buffer();
        auto clipboard = get_clipboard("CLIPBOARD");
        buffer->cut_clipboard(clipboard);
    }
    else
    {
        auto buffer = get_buffer();
        auto clipboard = get_clipboard("CLIPBOARD");
        buffer->copy_clipboard(clipboard);
    }
}

void Draw::copy()
{
    auto buffer = get_buffer();
    auto clipboard = get_clipboard("CLIPBOARD");
    buffer->copy_clipboard(clipboard);
}

void Draw::paste()
{
    bool isEditable = get_editable();
    if (isEditable)
    {
        auto buffer = get_buffer();
        auto pasteIter = buffer->get_iter_at_offset(0);
        auto clipboard = get_clipboard("CLIPBOARD");
        buffer->select_range(pasteIter, pasteIter);
        buffer->paste_clipboard(clipboard);
    }
}

/**
 * Process and parse each node in the AST
 */
void Draw::processNode(cmark_node *node, cmark_event_type ev_type)
{
    bool entering = (ev_type == CMARK_EVENT_ENTER);

    switch (node->type)
    {
    case CMARK_NODE_DOCUMENT:
        if (entering)
        {
            // Reset
            headingLevel = 0;
            bulletListLevel = 0;
            listLevel = 0;
        }
        break;

    case CMARK_NODE_BLOCK_QUOTE:
        break;

    case CMARK_NODE_LIST:
    {
        cmark_list_type listType = node->as.list.list_type;
        if (entering)
        {
            listLevel++;
        }
        else
        {
            // Last list level new line
            if (listLevel == 1)
            {
                insertText("\n");
            }
            listLevel--;
        }
        if (listLevel == 0)
        {
            // Reset bullet/ordered levels
            bulletListLevel = 0;
            orderedListLevel = 0;
            isOrderedList = false;
        }
        else if (listLevel > 0)
        {
            if (entering)
            {
                if (listType == cmark_list_type::CMARK_BULLET_LIST)
                {
                    bulletListLevel++;
                }
                else if (listType == cmark_list_type::CMARK_ORDERED_LIST)
                {
                    orderedListLevel++;
                    // Create the counter (and reset to zero)
                    orderedListCounters[orderedListLevel] = 0;
                }
            }
            else
            {
                // TODO: Un-indent list level again
                if (listType == cmark_list_type::CMARK_BULLET_LIST)
                {
                    bulletListLevel--;
                }
                else if (listType == cmark_list_type::CMARK_ORDERED_LIST)
                {
                    orderedListLevel--;
                }
            }

            isOrderedList = (orderedListLevel > 0) && (bulletListLevel <= 0);
        }
    }
    break;

    case CMARK_NODE_ITEM:
        // Line break for each item
        if (entering)
            insertText("\n");
        if (entering && isOrderedList)
        {
            // Increasement ordered list counter
            orderedListCounters[orderedListLevel]++;
        }
        break;

    case CMARK_NODE_HEADING:
        if (entering)
        {
            headingLevel = node->as.heading.level;
        }
        else
        {
            headingLevel = 0; // reset
        }
        break;

    case CMARK_NODE_CODE_BLOCK:
        break;

    case CMARK_NODE_HTML_BLOCK:
        break;

    case CMARK_NODE_CUSTOM_BLOCK:
        break;

    case CMARK_NODE_THEMATIC_BREAK:
    {
        insertText("\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015");
    }
    break;

    case CMARK_NODE_PARAGRAPH:
        if (listLevel == 0)
        {
            // insert new line, but not when listing is enabled
            insertText("\n");
        }
        break;

    case CMARK_NODE_TEXT:
    {
        // Instead of creating seperate pango layouts we may want to use Pango attributes,
        // for changing parts of the text. Which is most likely be faster.
        // https://developer.gnome.org/pango/stable/pango-Text-Attributes.html
        // Pango is using a simple parser for parsing (X)HTML:
        // https://developer.gnome.org/glib/stable/glib-Simple-XML-Subset-Parser.html
        // We can use simular parse functions and using their own 'OpenTag' struct containing a list of Pango attributes:
        // https://gitlab.gnome.org/GNOME/pango/-/blob/master/pango/pango-markup.c#L515

        std::string text = cmark_node_get_literal(node);
        // Insert tabs & bullet/number
        if (bulletListLevel > 0)
        {
            text.insert(0, std::string(bulletListLevel, '\u0009') + "\u2022 ");
        }
        else if (orderedListLevel > 0)
        {
            std::string number;
            if (orderedListLevel % 2 == 0)
            {
                number = Draw::intToRoman(orderedListCounters[orderedListLevel]) + " ";
            }
            else
            {
                number = std::to_string(orderedListCounters[orderedListLevel]) + ". ";
            }
            text.insert(0, std::string(orderedListLevel, '\u0009') + number);
        }

        // Unsert headings
        if (headingLevel > 0)
        {
            switch (headingLevel)
            {
            case 1:
                insertHeading1(text);
                break;
            case 2:
                insertHeading2(text);
                break;
            case 3:
                insertHeading3(text);
                break;
            case 4:
                insertHeading4(text);
                break;
            case 5:
                insertHeading5(text);
                break;
            case 6:
                insertHeading6(text);
                break;
            default:
                insertHeading5(text); // fallback
                break;
            }
        }
        // Bold/italic text
        else if (isBold && isItalic)
        {
            insertBoldItalic(text);
        }
        else if (isBold)
        {
            insertBold(text);
        }
        else if (isItalic)
        {
            insertItalic(text);
        }
        // URL
        else if (isLink)
        {
            insertLink(text, linkURL);
            linkURL = "";
        }
        else
        {
            // Normal text only
            insertText(text);
        }
    }
    break;

    case CMARK_NODE_LINEBREAK:
        // Hard brake
        insertText("\n");
        break;

    case CMARK_NODE_SOFTBREAK:
        // only insert space
        insertText(" ");
        break;

    case CMARK_NODE_CODE:
        break;

    case CMARK_NODE_HTML_INLINE:
        break;

    case CMARK_NODE_CUSTOM_INLINE:
        break;

    case CMARK_NODE_STRONG:
        isBold = entering;
        break;

    case CMARK_NODE_EMPH:
        isItalic = entering;
        break;

    case CMARK_NODE_LINK:
        isLink = entering;
        if (entering)
        {
            linkURL = cmark_node_get_url(node);
        }
        break;

    case CMARK_NODE_IMAGE:
        break;

    case CMARK_NODE_FOOTNOTE_REFERENCE:
        break;

    case CMARK_NODE_FOOTNOTE_DEFINITION:
        break;
    default:
        assert(false);
        break;
    }
}

void Draw::insertText(const std::string &text)
{
    insertMarkupText("<span font_desc=\"" + defaultFont.to_string() + "\">" + text + "</span>");
}

void Draw::insertHeading1(const std::string &text)
{
    insertMarkupText("\n<span font_desc=\"" + heading1.to_string() + "\">" + text + "</span>\n");
}

void Draw::insertHeading2(const std::string &text)
{
    insertMarkupText("\n<span font_desc=\"" + heading2.to_string() + "\">" + text + "</span>\n");
}

void Draw::insertHeading3(const std::string &text)
{
    insertMarkupText("\n<span font_desc=\"" + heading3.to_string() + "\">" + text + "</span>\n");
}

void Draw::insertHeading4(const std::string &text)
{
    insertMarkupText("\n<span font_desc=\"" + heading4.to_string() + "\">" + text + "</span>\n");
}

void Draw::insertHeading5(const std::string &text)
{
    insertMarkupText("\n<span font_desc=\"" + heading5.to_string() + "\">" + text + "</span>\n");
}

void Draw::insertHeading6(const std::string &text)
{
    insertMarkupText("\n<span foreground=\"gray\" font_desc=\"" + heading6.to_string() + "\">" + text + "</span>\n");
}

void Draw::insertBold(const std::string &text)
{
    insertMarkupText("<span font_desc=\"" + bold.to_string() + "\">" + text + "</span>");
}

void Draw::insertItalic(const std::string &text)
{
    insertMarkupText("<span font_desc=\"" + italic.to_string() + "\">" + text + "</span>");
}

void Draw::insertBoldItalic(const std::string &text)
{
    insertMarkupText("<span font_desc=\"" + boldItalic.to_string() + "\">" + text + "</span>");
}

void Draw::insertMarkupText(const std::string &text)
{
    DispatchData *data = g_new0(struct DispatchData, 1);
    data->buffer = buffer;
    data->text = text;
    gdk_threads_add_idle((GSourceFunc)insertTextIdle, data);
}

void Draw::insertLink(const std::string &text, const std::string &url)
{
    DispatchData *data = g_new0(struct DispatchData, 1);
    data->buffer = buffer;
    data->text = text;
    data->url = url;
    gdk_threads_add_idle((GSourceFunc)insertLinkIdle, data);
}

void Draw::clear()
{
    gdk_threads_add_idle((GSourceFunc)clearIdle, buffer);
}

/**
 * Insert text on Idle Call function
 */
gboolean Draw::insertTextIdle(struct DispatchData *data)
{
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(data->buffer, &end_iter);
    gtk_text_buffer_insert_markup(data->buffer, &end_iter, data->text.c_str(), -1);
    g_free(data);
    return FALSE;
}

/**
 * Insert link url on Idle Call function
 */
gboolean Draw::insertLinkIdle(struct DispatchData *data)
{
    GtkTextIter end_iter;
    GtkTextTag *tag;
    gtk_text_buffer_get_end_iter(data->buffer, &end_iter);
    tag = gtk_text_buffer_create_tag(data->buffer, NULL,
                                     "foreground", "blue",
                                     "underline", PANGO_UNDERLINE_SINGLE,
                                     NULL);
    g_object_set_data(G_OBJECT(tag), "url", g_strdup(data->url.c_str()));
    gtk_text_buffer_insert_with_tags(data->buffer, &end_iter, data->text.c_str(), -1, tag, NULL);
    g_free(data);
    return FALSE;
}

/**
 * Clear Text on Idle Call function
 */
gboolean Draw::clearIdle(GtkTextBuffer *textBuffer)
{
    GtkTextIter start_iter, end_iter;
    gtk_text_buffer_get_start_iter(textBuffer, &start_iter);
    gtk_text_buffer_get_end_iter(textBuffer, &end_iter);
    gtk_text_buffer_delete(textBuffer, &start_iter, &end_iter);
    return FALSE;
}

/**
 * Convert number to roman numerals
 */
std::string const Draw::intToRoman(int num)
{
    static const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static const std::string numerals[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    std::string res;
    for (int i = 0; i < 13; ++i)
    {
        while (num >= values[i])
        {
            num -= values[i];
            res += numerals[i];
        }
    }
    return res;
}