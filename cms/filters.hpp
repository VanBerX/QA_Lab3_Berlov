#ifndef FILTERS_HPP_SENTRY
#define FILTERS_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

class FilterChain {
    class StreamFilter *chain, *last;
    bool finished;
public:
    FilterChain() : chain(0), last(0), finished(false) {}
    ~FilterChain();

    void Add(StreamFilter *f);

    bool Empty() const { return !chain; }
    ScriptVariable operator()(const ScriptVariable &src) const;
};

enum parconv_mode { parconv_none, parconv_webstyle, parconv_texstyle };

class FilterChainSet {
    FilterChain data, userdata, content, enc_only;
public:
    FilterChainSet();
    ~FilterChainSet() {}

    void AddFromUtf(const int * const *tbl);
    void AddToUtf(const int *tbl);
    void AddNewlineToParagraphConv(bool texstyle); // ONLY when no TagFilter!
    void AddTagFilter(const char * const *tags,
                      const char * const *attrs,
                      int parconv);

    ScriptVariable ConvertData(const ScriptVariable &src) const;
    ScriptVariable ConvertUserdata(const ScriptVariable &src) const;
    ScriptVariable ConvertContent(const ScriptVariable &src) const;
    ScriptVariable ConvertEncOnly(const ScriptVariable &src) const;

private:
    ScriptVariable DoConvert(const FilterChain *fc,
                             const ScriptVariable &src) const;
};

class FilterChainMaker {
    int target_encoding;
    const int * const *utf_to_target_table; // 0 means target is utf
    char **allowed_tags, **allowed_attrs;   // note these fields are owned!
    const char *errmsg;
public:
    FilterChainMaker(const char *target_enc,
                     const char *tags, const char *attrs);
    ~FilterChainMaker();

    FilterChainSet *MakeChainSet(const char *src_enc,
                                 int par, bool tags) const;
    FilterChainSet *MakeChainSet(const char *enc, const char *fmt) const;
    FilterChainSet *MakeChainSet(const ScriptVector &headers_dict) const;

    const char *ErrMsg() const { return errmsg; }
};

#endif
