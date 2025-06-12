#include <stfilter/stfilter.hpp>
#include <stfilter/stfhtml.hpp>

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>

#include "filters.hpp"

/* ``Destination'' for filter chains, which uses
   a ScriptVarable object as the result storage
 */
class DestSV : public StreamFilter {
    ScriptVariable *the_dest;
public:
    DestSV() : StreamFilter(0), the_dest(0) {}
    ~DestSV() {}
    void SetDest(ScriptVariable *sv) { the_dest = sv; }
private:
    virtual void FeedChar(int c)
        { the_dest->operator+=((char)c); } // no EOF, ok
};


FilterChain::~FilterChain()
{
    if(chain) {
        chain->DeleteChain();
        // DON'T!   /* delete chain; */ it is already done by DeleteChain!
    }
}

void FilterChain::Add(StreamFilter *f)
{
    if(last) {
        last->AddToEnd(f);
        last = f;
    } else {
        chain = f;
        last = f;
    }
}

ScriptVariable FilterChain::operator()(const ScriptVariable &src) const
{
    if(!chain)
        return src;
    if(!finished) {
        StreamFilter *p = new DestSV;
        const_cast<FilterChain*>(this)->Add(p);
        const_cast<FilterChain*>(this)->finished = true;
    }
    ScriptVariable res;
    static_cast<DestSV*>(last)->SetDest(&res);
    chain->ChainReset();
    const char *zstr = src.c_str();
    for(; *zstr; zstr++)
        chain->FeedChar(*zstr);
    chain->FeedEnd();
    static_cast<DestSV*>(last)->SetDest(0);
    return res;
}



FilterChainSet::FilterChainSet()
{
    userdata.Add(new StreamFilterHtmlProtect(0));
}

void FilterChainSet::AddFromUtf(const int * const *tbl)
{
    data.Add(new StreamFilterUtf8ToHtml(tbl, 0));
    userdata.Add(new StreamFilterUtf8ToHtml(tbl, 0));
    content.Add(new StreamFilterUtf8ToHtml(tbl, 0));
    enc_only.Add(new StreamFilterUtf8ToHtml(tbl, 0));
}

void FilterChainSet::AddToUtf(const int *tbl)
{
    data.Add(new StreamFilterExtAsciiToUtf8(tbl, 0));
    userdata.Add(new StreamFilterExtAsciiToUtf8(tbl, 0));
    content.Add(new StreamFilterExtAsciiToUtf8(tbl, 0));
    enc_only.Add(new StreamFilterExtAsciiToUtf8(tbl, 0));
}

void FilterChainSet::AddNewlineToParagraphConv(bool texstyle)
{
    content.Add(new StreamFilterHtmlReplaceNL(texstyle, 0));
}

/* actually, the following tags are not the "space preserving",
   they rather are the tags inside which the newline to paragraph
   conversion must be disabled AND which can't be contained in a
   paragraph; this, well, must be rewritten
 */
static const char * const the_space_preserving_tags[] = {
    "pre", "ul", "ol", "table", "p", "blockquote",
    /* "cite", "em", "strong", "i", "span", -- NO!!! */
    "h1", "h2", "h3", "h4", "h5", "h6", 0
};

void FilterChainSet::AddTagFilter(const char * const *tags,
                                  const char * const *attrs,
                                  int parconv)
{
    StreamFilterHtmlTags *p = new StreamFilterHtmlTags(tags, attrs, 0);
    content.Add(p);

    switch(parconv) {
    case parconv_none:
        break;
    case parconv_webstyle:
        p->AddControlledNLReplacer(the_space_preserving_tags, false);
        break;
    case parconv_texstyle:
        p->AddControlledNLReplacer(the_space_preserving_tags, true);
        break;
    }
}

ScriptVariable
FilterChainSet::ConvertData(const ScriptVariable &src) const
{
    return DoConvert(&data, src);
}

ScriptVariable
FilterChainSet::ConvertUserdata(const ScriptVariable &src) const
{
    return DoConvert(&userdata, src);
}

ScriptVariable
FilterChainSet::ConvertContent(const ScriptVariable &src) const
{
    return DoConvert(&content, src);
}

ScriptVariable
FilterChainSet::ConvertEncOnly(const ScriptVariable &src) const
{
    return DoConvert(&enc_only, src);
}

ScriptVariable FilterChainSet::DoConvert(const FilterChain *fc,
                                         const ScriptVariable &src) const
{
    if(fc->Empty())
        return src;
    return (*fc)(src);
}


FilterChainMaker::FilterChainMaker(const char *target_enc,
                                   const char *tags, const char *attrs)
    : allowed_tags(0), allowed_attrs(0), errmsg(0)
{
    if(!target_enc || !*target_enc) {
        target_encoding = streamfilter_enc_unknown;  // disable transcodings
    } else {
        target_encoding = streamfilter_find_encoding(target_enc);
        if(target_encoding == streamfilter_enc_unknown) {
            errmsg = "unknown target encoding";
            utf_to_target_table = 0;
        } else
        if(target_encoding == streamfilter_enc_utf8) {
            utf_to_target_table = 0;
        } else {
            utf_to_target_table =
                StreamFilterUtf8ToExtAscii::GetTable(target_encoding);
        }
    }
    if(tags && *tags) {
        ScriptWordVector tv(tags);
        allowed_tags = tv.MakeArgv();
    }
    if(attrs && *attrs) {
        ScriptWordVector av(attrs);
        allowed_attrs = av.MakeArgv();
    }
}

FilterChainMaker::~FilterChainMaker()
{
    if(allowed_tags)
        ScriptVector::DeleteArgv(allowed_tags);
    if(allowed_attrs)
        ScriptVector::DeleteArgv(allowed_attrs);
}

// NOTE as of now it is unexpected for this method to return 0
FilterChainSet* FilterChainMaker::
MakeChainSet(const char *src_enc, int par, bool tags) const
{
    FilterChainSet *res = new FilterChainSet;

    //if(target_encoding != streamfilter_enc_utf8 && src_enc && *src_enc) {

    if(src_enc && *src_enc) {
        int enc_code = streamfilter_find_encoding(src_enc);
        const int *tbl = 0;
        if(enc_code != target_encoding) {
            if(enc_code != streamfilter_enc_utf8)
                tbl = StreamFilterExtAsciiToUtf8::GetTable(enc_code);
            if(tbl)
                res->AddToUtf(tbl);
            if(utf_to_target_table && (tbl || enc_code==streamfilter_enc_utf8))
                res->AddFromUtf(utf_to_target_table);
        }
    }

#if 0
    if(!res && !par && !tags)
        return 0;

    if(!res)
        res = new FilterChain;
#endif

    if(par || tags)
        res->AddTagFilter(tags?allowed_tags:0, tags?allowed_attrs:0, par);
    return res;
}

FilterChainSet* FilterChainMaker::
MakeChainSet(const char *encoding, const char *format) const
{
    ScriptTokenVector fmt_tokens(format, ",", " \t\r\n");
    int nlconv = parconv_none;
    bool tagconv = false;
    int i;
    for(i = 0; i < fmt_tokens.Length(); i++) {
        fmt_tokens[i].Tolower();
        if(fmt_tokens[i] == "breaks")
            nlconv = parconv_webstyle;
        else
        if(fmt_tokens[i] == "texbreaks")
            nlconv = parconv_texstyle;
        else
        if(fmt_tokens[i] == "tags")
            tagconv = true;
        if(tagconv && nlconv)
            break;
    }
    return MakeChainSet(encoding, nlconv, tagconv);
}

FilterChainSet* FilterChainMaker::MakeChainSet(const ScriptVector &hdr) const
{
    ScriptVariable encoding, format;
    bool enc_found = false, fmt_found = false;
    int i;
    for(i = 0; i < hdr.Length()-1; i+=2) {
        if(hdr[i] == "encoding") {
            encoding = hdr[i+1];
            enc_found = true;
        } else
        if(hdr[i] == "format") {
            format = hdr[i+1];
            fmt_found = true;
        }
        if(enc_found && fmt_found)
            break;
    }
    return MakeChainSet(encoding.c_str(), format.c_str());
}
