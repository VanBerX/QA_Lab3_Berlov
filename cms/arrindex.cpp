#include <scriptpp/scrmacro.hpp>

#include "database.hpp"
#include "arrindex.hpp"



struct arrindex_context {
    const IndexBarStyle *style;
    const ArrayData *data;
    const ScriptMacroprocessor *macroproc;
    ScriptVector v;    /* exactly 3 items: text, link, anchor */
    ScriptVariable result;
};

#define PROC(field) \
    do {\
        ctx.result += ctx.macroproc->Process(ctx.style->field, ctx.v);\
    } while(0)

static bool it_is_leftmost(const ArrayData &d)
{
    return d.reverse ?
        (d.current_page == d.page_count || d.current_page < 1) :
        (d.current_page <= 1);
}
static bool it_is_rightmost(const ArrayData &d)
{
    return d.reverse ?
        (d.current_page == 1) :
        (d.current_page == d.page_count);
}
static int prev_to_main_page(const ArrayData &d)
{
    //return d.page_count - ((d.item_count % d.items_per_page == 0) ? 0 : 1);
    //return d.page_count - (d.has_underfilled_page ? 1 : 0);
    return d.page_count - 1;
}

static ScriptVariable ending_link(const ArrayData *data, bool left)
{
    bool big_end = (left == data->reverse);
    if(big_end && data->href_last.IsValid())
        return data->href_last;
    if(!big_end && data->href_first.IsValid())
        return data->href_first;

    int idx = big_end ? data->page_count : 1;
    return data->href_array[idx];
}
static ScriptVariable leftmost_link(const ArrayData *data)
{
    return ending_link(data, true);
}
static ScriptVariable rightmost_link(const ArrayData *data)
{
    return ending_link(data, false);
}

static void add_left_end(arrindex_context &ctx)
{
    bool leftmost = it_is_leftmost(*ctx.data);

        // the '|<<' link
    ctx.v[0] = ctx.style->textfirst;
    bool left_end_enabled =
        !leftmost || (ctx.data->reverse && ctx.data->current_page > 1);
    if(left_end_enabled)
        ctx.v[1] = leftmost_link(ctx.data);
    else
        ctx.v[1].Invalidate();

    if(left_end_enabled)
        PROC(link);
    else
        PROC(greylink);

        // the '<<' link
    ctx.v[0] = ctx.style->textprev;
    if(leftmost || (ctx.data->current_page <= 0 && ctx.data->reverse)) {
        PROC(greylink);
    } else {
        int ppg;
        if(ctx.data->current_page > 0)
            ppg = ctx.data->current_page + (ctx.data->reverse ? 1 : -1);
        else
            ppg = prev_to_main_page(*ctx.data);  // NB: reverse is false!
        if(ppg < 2 && ctx.data->href_first.IsValid())
            ctx.v[1] = ctx.data->href_first;
        else
            ctx.v[1] = ctx.data->href_array[ppg];
        PROC(link);
    }
}

static void add_right_end(arrindex_context &ctx)
{
    bool rightmost = it_is_rightmost(*ctx.data);
    bool rev = ctx.data->reverse;

        // the '>>' link
    ctx.v[0] = ctx.style->textnext;
    if(rightmost) {
        PROC(greylink);
    } else {
        int ppg;
        if(ctx.data->current_page > 0)
            ppg = ctx.data->current_page + (rev ? -1 : 1);
        else
            ppg = rev ? prev_to_main_page(*ctx.data) : 2;
        if(!rev && ppg < 2 && ctx.data->href_first.IsValid())
            ctx.v[1] = ctx.data->href_first;
        else
            ctx.v[1] = ctx.data->href_array[ppg];
        PROC(link);
    }

        // the '>>|' link
    ctx.v[0] = ctx.style->textlast;
    if(rightmost) {
        PROC(greylink);
    } else {
        ctx.v[1] = rightmost_link(ctx.data);
        PROC(link);
    }
}

static int range_len(int start, int end)
{
    return (start < end ? end - start : start - end) + 1;
}

static void do_add_range(int start, int end, bool rev, arrindex_context &ctx)
{
    //printf("DO_ADD_RANGE %d %d %s\n", start, end, rev ? "REV" : "");
    int i;
    for(i = start; rev ? (i >= end) : (i <= end); rev ? i-- : i++) {
        ctx.v[0] = ScriptNumber(i);
        if(i <= 1 && ctx.data->href_first.IsValid())
            ctx.v[1] = ctx.data->href_first;
        else
            ctx.v[1] = ctx.data->href_array[i];
        PROC(link);
    }
}

static void add_range(int start, int end, bool rev, arrindex_context &ctx)
{
    //printf("\nADD_RANGE %d %d %s\n", start, end, rev ? "REV" : "");
    if(rev ? (start < end) : (start > end))
        return;
    int len = range_len(start, end);
    int ts = ctx.style->tailsize;
    //printf("     len = %d, ts = %d\n", len, ts);
    if(len <= 2 * ts + 1) {
        do_add_range(start, end, rev, ctx);
        return;
    }
    ts--;
    do_add_range(start, start + (rev ? -ts : ts), rev, ctx);
    ctx.v[0] = "";
    ctx.v[1] = "";
    PROC(textbreak);
    do_add_range(end + (rev ? ts : -ts), end, rev, ctx);
}

static void add_currentpage(arrindex_context &ctx)
{
    ctx.v[0] = ScriptNumber(ctx.data->current_page);
    ctx.v[1] = "";
    PROC(curpos);
}

ScriptVariable build_array_index(const IndexBarStyle &style,
                                 const ArrayData &data,
                                 const ScriptMacroprocessor &sub,
                                 const ScriptVariable &anchor)
{
    if(data.page_count < 2)
        return "";

    arrindex_context ctx;
    ctx.style = &style;
    ctx.data = &data;
    ctx.macroproc = &sub;
    ctx.v[0] = "";
    ctx.v[1] = "";
    ctx.v[2] = anchor;
    ctx.result = "";

    PROC(begin);
    add_left_end(ctx);

    if(ctx.data->current_page <= 0) {  // no "currentpage" element
        if(ctx.data->reverse)
            add_range(ctx.data->page_count, 1, true, ctx);
        else
            add_range(1, ctx.data->page_count, false, ctx);
    } else {
        if(ctx.data->reverse) {
            add_range(ctx.data->page_count,ctx.data->current_page+1,true,ctx);
            add_currentpage(ctx);
            add_range(ctx.data->current_page-1, 1, true, ctx);
        } else {
            add_range(1, ctx.data->current_page-1, false, ctx);
            add_currentpage(ctx);
            add_range(ctx.data->current_page+1,ctx.data->page_count,false,ctx);
        }
    }

    add_right_end(ctx);
    PROC(end);
    return ctx.result;
}
