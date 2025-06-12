#ifndef ARRINDEX_HPP_SENTRY
#define ARRINDEX_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>

/*
    IndexBarStyle contains raw information retrieved from the
    [indexbar XXX] configuration file section.  It is owned by
    the dbsubst module (ArrayIndex class object).  It is filled
    by the appropriate method of the database.

    ArrayData is owned and filled by the generator.
    The generator "installs" and "uninstalls" instances of
    the structure with the database object, as necessary.

 */

struct IndexBarStyle {
    ScriptVariable begin, end, textbreak, link, greylink, curpos,
        textprev, textnext, textfirst, textlast;
    int tailsize;
};

struct ArrayData {
    int page_count; // it's always from 1 to page_count,
                    // ``main'' page isn't reflected here
    bool reverse;
#if 0
    bool has_underfilled_page;  // only used for reverse case to determine
         // where to jump from the 'main' page in case 'prev' is clicked
#endif
    ScriptVector href_array;
    ScriptVariable href_first, href_last;
         // links for '<<'  and  '>>'
         // (href_first for the little end, href_last for the big end)
         // leave them 'invalid' to use the same links as '1' or 'N'
         // used to get to the 'main' page instead of the numeric,
         // and this is the only way to do so
    int current_page;
    ArrayData() { href_first.Invalidate(); href_last.Invalidate(); }
};

#if 0
struct IndexInformation {
    ScriptVariable begin, end, textbreak, link, greylink, curpos,
        textprev, textnext, textfirst, textlast;
    int tailsize;
    ScriptVariable anchor;
    class ScriptMacroprocessor *macroproc;
};
#endif

class ScriptMacroprocessor;

ScriptVariable build_array_index(const IndexBarStyle &style,
                                 const ArrayData &data,
                                 const ScriptMacroprocessor &sub,
                                 const ScriptVariable &anchor);

#endif
